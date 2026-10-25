#include "../machine/internal.hpp"

#include <bit>
#include <cstdint>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

bool is_atomic(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom || is_int(cell) || cell.tag() == Tag::Float;
}

bool is_compound(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

bool is_nil(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom && machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3)) == "[]";
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

void instantiation(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

bool acyclic_term(const Impl& machine, Cell cell, std::vector<std::uint64_t>& seen) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell) || is_atomic(machine, cell)) return true;
  for (const auto raw : seen) {
    if (raw == cell.raw()) return false;
  }
  seen.push_back(cell.raw());
  if (cell.tag() == Tag::Lis) {
    return acyclic_term(machine, machine.heap.list_head(cell), seen) &&
           acyclic_term(machine, machine.heap.list_tail(cell), seen);
  }
  if (cell.tag() != Tag::Str) return true;
  for (std::uint16_t index = 0; index < machine.heap.functor_arity(cell); ++index) {
    if (!acyclic_term(machine, machine.heap.arg(cell, index), seen)) return false;
  }
  return true;
}

bool proper_list(const Impl& machine, Cell cell) {
  std::vector<std::uint64_t> spine;
  Cell cursor = cell;
  while (true) {
    cursor = machine.heap.deref(cursor);
    if (machine.heap.unbound(cursor)) return false;
    if (is_nil(machine, cursor)) break;
    if (cursor.tag() != Tag::Lis) return false;
    for (const auto raw : spine) {
      if (raw == cursor.raw()) return false;
    }
    spine.push_back(cursor.raw());
    cursor = machine.heap.list_tail(cursor);
  }
  std::vector<std::uint64_t> seen;
  return acyclic_term(machine, cell, seen);
}

bool occurs_in(const Impl& machine, Cell var, Cell term, std::vector<std::uint64_t>& seen) {
  if (term.raw() == var.raw()) return true;
  term = machine.heap.deref(term);
  if (term.raw() == var.raw()) return true;
  if (machine.heap.unbound(term) || (term.tag() != Tag::Lis && term.tag() != Tag::Str)) return false;
  for (const auto raw : seen) {
    if (raw == term.raw()) return false;
  }
  seen.push_back(term.raw());
  if (term.tag() == Tag::Lis) {
    return occurs_in(machine, var, machine.heap.list_head(term), seen) ||
           occurs_in(machine, var, machine.heap.list_tail(term), seen);
  }
  const auto arity = machine.heap.functor_arity(term);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (occurs_in(machine, var, machine.heap.arg(term, index), seen)) return true;
  }
  return false;
}

void occurs_unify(Impl& machine, const Goal& goal) {
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  if (!do_unify(machine, goal.arg(machine, 0), goal.arg(machine, 1))) {
    machine.failed = true;
    return;
  }
  for (std::size_t index = trail_mark; index < machine.trail.slots.size(); ++index) {
    const Cell var{std::bit_cast<std::uint64_t>(machine.trail.slots[index])};
    std::vector<std::uint64_t> seen;
    if (occurs_in(machine, var, machine.heap.deref(var), seen)) {
      restore_state(machine, heap_mark, trail_mark);
      machine.failed = true;
      return;
    }
  }
  succeed(machine, goal);
}

bool typed(Impl& machine, const Goal& goal, bool (*test)(const Impl&, Cell)) {
  if (test(machine, goal.arg(machine, 0))) succeed(machine, goal);
  else machine.failed = true;
  return true;
}

}  // namespace

bool run_types(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (goal.arity != 1 && !(goal.arity == 2 && (name == "unify_with_occurs_check"))) return false;
  if (name == "var" && goal.arity == 1) {
    if (machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "nonvar" && goal.arity == 1) {
    if (!machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "atom" && goal.arity == 1) {
    const Cell value = machine.heap.deref(goal.arg(machine, 0));
    if (value.tag() == Tag::Atom) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "integer" && goal.arity == 1) {
    if (is_int(machine.heap.deref(goal.arg(machine, 0)))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "float" && goal.arity == 1) {
    if (machine.heap.deref(goal.arg(machine, 0)).tag() == Tag::Float) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "atomic" && goal.arity == 1) return typed(machine, goal, is_atomic);
  if (name == "compound" && goal.arity == 1) return typed(machine, goal, is_compound);
  if (name == "number" && goal.arity == 1) {
    const Cell value = machine.heap.deref(goal.arg(machine, 0));
    if (is_int(value) || value.tag() == Tag::Float) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "callable" && goal.arity == 1) {
    const Cell value = machine.heap.deref(goal.arg(machine, 0));
    if (machine.heap.unbound(value)) {
      instantiation(machine, goal);
      return true;
    }
    if (value.tag() == Tag::Atom || is_compound(machine, value)) succeed(machine, goal);
    else raise_formal(machine, formal_type(machine, "callable", value), goal.name, goal.arity);
    return true;
  }
  if (name == "is_list" && goal.arity == 1) {
    if (proper_list(machine, goal.arg(machine, 0))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "unify_with_occurs_check" && goal.arity == 2) {
    occurs_unify(machine, goal);
    return true;
  }
  return false;
}

}
