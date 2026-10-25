#include "../machine/internal.hpp"

#include <string_view>
#include <vector>

namespace mosa {
namespace {

constexpr std::uint16_t kMaxArity = 2047;

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

bool is_atomic(Cell cell) {
  return cell.tag() == Tag::Atom || is_int(cell) || cell.tag() == Tag::Float;
}

bool is_compound(Cell cell) { return cell.tag() == Tag::Str || cell.tag() == Tag::Lis; }

bool is_nil(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Atom && machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3)) == "[]";
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

void instantiation(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

Cell list_from(Impl& machine, const std::vector<Cell>& items) {
  Cell tail = machine.heap.atom(machine.atoms.intern("[]"));
  for (std::size_t index = items.size(); index > 0; --index) {
    tail = machine.heap.list(items[index - 1], tail);
  }
  return tail;
}

void decompose_functor(Impl& machine, const Goal& goal, Cell term) {
  Cell name = machine.heap.atom(0);
  Cell arity = machine.heap.integer(0);
  if (term.tag() == Tag::Atom || is_int(term) || term.tag() == Tag::Float) {
    name = term.tag() == Tag::Atom ? term : term;
    if (term.tag() == Tag::Atom) name = term;
    else if (is_int(term)) name = machine.heap.integer(machine.heap.integer_value(term));
    else name = machine.heap.floating(machine.heap.float_value(term));
    arity = machine.heap.integer(0);
  } else if (term.tag() == Tag::Lis) {
    name = machine.heap.atom(machine.atoms.intern("."));
    arity = machine.heap.integer(2);
  } else {
    name = machine.heap.atom(machine.heap.functor_atom(term));
    arity = machine.heap.integer(machine.heap.functor_arity(term));
  }
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  if (!do_unify(machine, goal.arg(machine, 1), name) || !do_unify(machine, goal.arg(machine, 2), arity)) {
    restore_state(machine, heap_mark, trail_mark);
    machine.failed = true;
    return;
  }
  succeed(machine, goal);
}

bool is_dot_name(const Impl& machine, Cell name) {
  name = machine.heap.deref(name);
  return name.tag() == Tag::Atom && machine.atoms.name(atom_id(name)) == ".";
}

Cell make_term(Impl& machine, Cell name, const std::vector<Cell>& args) {
  if (args.size() == 2 && is_dot_name(machine, name)) return machine.heap.list(args[0], args[1]);
  return machine.heap.structure(atom_id(name), args);
}

void construct_functor(Impl& machine, const Goal& goal, Cell name, std::int64_t arity) {
  if (arity == 0) {
    if (!do_unify(machine, goal.arg(machine, 0), name)) machine.failed = true;
    else succeed(machine, goal);
    return;
  }
  std::vector<Cell> args(static_cast<std::size_t>(arity));
  for (auto& arg : args) arg = machine.heap.fresh_ref();
  const Cell made = make_term(machine, name, args);
  if (!do_unify(machine, goal.arg(machine, 0), made)) machine.failed = true;
  else succeed(machine, goal);
}

void run_functor(Impl& machine, const Goal& goal) {
  const Cell term = machine.heap.deref(goal.arg(machine, 0));
  const Cell name = machine.heap.deref(goal.arg(machine, 1));
  const Cell arity_cell = machine.heap.deref(goal.arg(machine, 2));
  const bool term_var = machine.heap.unbound(term);
  const bool name_var = machine.heap.unbound(name);
  const bool arity_var = machine.heap.unbound(arity_cell);
  if (term_var && (name_var || arity_var)) {
    instantiation(machine, goal);
    return;
  }
  if (!term_var) {
    if (!arity_var && !is_int(arity_cell)) {
      raise_formal(machine, formal_type(machine, "integer", arity_cell), goal.name, goal.arity);
      return;
    }
    decompose_functor(machine, goal, term);
    return;
  }
  if (!is_atomic(name)) {
    raise_formal(machine, formal_type(machine, "atomic", name), goal.name, goal.arity);
    return;
  }
  if (!is_int(arity_cell)) {
    raise_formal(machine, formal_type(machine, "integer", arity_cell), goal.name, goal.arity);
    return;
  }
  const auto arity = machine.heap.integer_value(arity_cell);
  if (arity < 0) {
    raise_formal(machine, formal_domain(machine, "not_less_than_zero", arity_cell), goal.name, goal.arity);
    return;
  }
  if (arity > kMaxArity) {
    raise_formal(machine, formal_representation(machine, "max_arity"), goal.name, goal.arity);
    return;
  }
  if (arity > 0 && name.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", name), goal.name, goal.arity);
    return;
  }
  construct_functor(machine, goal, name, arity);
}

void run_arg(Impl& machine, const Goal& goal) {
  const Cell index = machine.heap.deref(goal.arg(machine, 0));
  const Cell term = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(index) || machine.heap.unbound(term)) {
    instantiation(machine, goal);
    return;
  }
  if (!is_int(index)) {
    raise_formal(machine, formal_type(machine, "integer", index), goal.name, goal.arity);
    return;
  }
  const auto number = machine.heap.integer_value(index);
  if (number < 0) {
    raise_formal(machine, formal_domain(machine, "not_less_than_zero", index), goal.name, goal.arity);
    return;
  }
  if (!is_compound(term)) {
    raise_formal(machine, formal_type(machine, "compound", term), goal.name, goal.arity);
    return;
  }
  const auto arity = term.tag() == Tag::Lis ? std::int64_t{2} : machine.heap.functor_arity(term);
  if (number == 0 || number > arity) {
    machine.failed = true;
    return;
  }
  Cell arg = term.tag() == Tag::Lis ? (number == 1 ? machine.heap.list_head(term) : machine.heap.list_tail(term))
                                    : machine.heap.arg(term, static_cast<std::size_t>(number - 1));
  if (!do_unify(machine, goal.arg(machine, 2), arg)) machine.failed = true;
  else succeed(machine, goal);
}

enum class ListRead { Ok, Partial, Bad };

ListRead proper_items(Impl& machine, Cell list, std::vector<Cell>& items) {
  std::vector<std::uint64_t> seen;
  while (true) {
    list = machine.heap.deref(list);
    if (machine.heap.unbound(list)) return ListRead::Partial;
    if (is_nil(machine, list)) return ListRead::Ok;
    if (list.tag() != Tag::Lis) return ListRead::Bad;
    for (const auto raw : seen) {
      if (raw == list.raw()) return ListRead::Bad;
    }
    seen.push_back(list.raw());
    items.push_back(machine.heap.list_head(list));
    list = machine.heap.list_tail(list);
  }
}

void univ_from_term(Impl& machine, const Goal& goal, Cell term) {
  std::vector<Cell> items;
  if (term.tag() == Tag::Atom || is_int(term) || term.tag() == Tag::Float) items.push_back(term);
  else if (term.tag() == Tag::Lis) {
    items.push_back(machine.heap.atom(machine.atoms.intern(".")));
    items.push_back(machine.heap.list_head(term));
    items.push_back(machine.heap.list_tail(term));
  } else if (term.tag() == Tag::Str) {
    items.push_back(machine.heap.atom(machine.heap.functor_atom(term)));
    for (std::uint16_t index = 0; index < machine.heap.functor_arity(term); ++index) {
      items.push_back(machine.heap.arg(term, index));
    }
  } else {
    machine.failed = true;
    return;
  }
  if (!do_unify(machine, goal.arg(machine, 1), list_from(machine, items))) machine.failed = true;
  else succeed(machine, goal);
}

void run_univ(Impl& machine, const Goal& goal) {
  const Cell term = machine.heap.deref(goal.arg(machine, 0));
  const Cell list = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(term) && machine.heap.unbound(list)) {
    instantiation(machine, goal);
    return;
  }
  if (!machine.heap.unbound(term)) {
    univ_from_term(machine, goal, term);
    return;
  }
  if (is_nil(machine, list)) {
    raise_formal(machine, formal_domain(machine, "non_empty_list", list), goal.name, goal.arity);
    return;
  }
  std::vector<Cell> items;
  const ListRead read = proper_items(machine, list, items);
  if (read == ListRead::Partial) {
    instantiation(machine, goal);
    return;
  }
  if (read == ListRead::Bad) {
    raise_formal(machine, formal_type(machine, "list", list), goal.name, goal.arity);
    return;
  }
  const Cell head = machine.heap.deref(items[0]);
  if (machine.heap.unbound(head)) {
    instantiation(machine, goal);
    return;
  }
  if (!is_atomic(head)) {
    raise_formal(machine, formal_type(machine, "atomic", head), goal.name, goal.arity);
    return;
  }
  const auto argc = static_cast<std::int64_t>(items.size() - 1);
  if (argc > kMaxArity) {
    raise_formal(machine, formal_representation(machine, "max_arity"), goal.name, goal.arity);
    return;
  }
  if (argc > 0 && head.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", head), goal.name, goal.arity);
    return;
  }
  Cell made = head;
  if (argc > 0) {
    std::vector<Cell> args(items.begin() + 1, items.end());
    made = make_term(machine, head, args);
  }
  if (!do_unify(machine, goal.arg(machine, 0), made)) machine.failed = true;
  else succeed(machine, goal);
}

void collect_vars(const Impl& machine, Cell term, std::vector<Cell>& vars, std::vector<std::uint64_t>& seen) {
  if (term.tag() == Tag::Ref) {
    const Cell value = machine.heap.deref(term);
    if (machine.heap.unbound(value)) {
      for (const Cell& var : vars) {
        if (var.raw() == value.raw()) return;
      }
      vars.push_back(value);
      return;
    }
    term = value;
  } else {
    term = machine.heap.deref(term);
  }
  if (machine.heap.unbound(term) || (term.tag() != Tag::Lis && term.tag() != Tag::Str)) return;
  for (const auto raw : seen) {
    if (raw == term.raw()) return;
  }
  seen.push_back(term.raw());
  if (term.tag() == Tag::Lis) {
    collect_vars(machine, machine.heap.list_head(term), vars, seen);
    collect_vars(machine, machine.heap.list_tail(term), vars, seen);
    return;
  }
  for (std::uint16_t index = 0; index < machine.heap.functor_arity(term); ++index) {
    collect_vars(machine, machine.heap.arg(term, index), vars, seen);
  }
}

void run_copy(Impl& machine, const Goal& goal) {
  const ExportedTerm copy = export_term(machine, goal.arg(machine, 0));
  const Cell rebuilt = import_term(machine, copy);
  if (!do_unify(machine, goal.arg(machine, 1), rebuilt)) machine.failed = true;
  else succeed(machine, goal);
}

void run_variables(Impl& machine, const Goal& goal) {
  const Cell bag = machine.heap.deref(goal.arg(machine, 1));
  if (!machine.heap.unbound(bag) && bag.tag() != Tag::Lis && !is_nil(machine, bag)) {
    raise_formal(machine, formal_type(machine, "list", bag), goal.name, goal.arity);
    return;
  }
  std::vector<Cell> vars;
  std::vector<std::uint64_t> seen;
  collect_vars(machine, goal.arg(machine, 0), vars, seen);
  if (!do_unify(machine, goal.arg(machine, 1), list_from(machine, vars))) machine.failed = true;
  else succeed(machine, goal);
}

}  // namespace

bool run_terms(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "functor" && goal.arity == 3) {
    run_functor(machine, goal);
    return true;
  }
  if (name == "arg" && goal.arity == 3) {
    run_arg(machine, goal);
    return true;
  }
  if (name == "=.." && goal.arity == 2) {
    run_univ(machine, goal);
    return true;
  }
  if (name == "copy_term" && goal.arity == 2) {
    run_copy(machine, goal);
    return true;
  }
  if (name == "term_variables" && goal.arity == 2) {
    run_variables(machine, goal);
    return true;
  }
  return false;
}

}
