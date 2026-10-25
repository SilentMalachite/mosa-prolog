#include "internal.hpp"

#include <bit>
#include <cstdint>

namespace mosa {
namespace {

const std::uint64_t* watermark(const Impl& machine) {
  if (machine.stack.empty()) return machine.heap.data();
  return machine.heap.data() + machine.stack.back().heap_mark;
}

}  // namespace

void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark) {
  while (machine.trail.slots.size() > trail_mark) {
    auto* slot = machine.trail.slots.back();
    machine.trail.slots.pop_back();
    *slot = std::bit_cast<std::uint64_t>(slot);
  }
  machine.heap.truncate(heap_mark);
}

bool do_unify(Impl& machine, Cell left, Cell right) {
  machine.trail.mark = watermark(machine);
  return unify(machine.heap, machine.trail, machine.trail.slots.size(), machine.heap.size(), left, right);
}

void enter_clause(Impl& machine, const Clause& clause, std::uint64_t goal, std::size_t cut) {
  std::vector<Cell> env;
  env.reserve(clause.vars);
  for (std::uint16_t index = 0; index < clause.vars; ++index) env.push_back(machine.heap.fresh_ref());
  if (clause.head.tag == IrTag::Fun) {
    for (std::size_t index = 0; index < clause.head.args.size(); ++index) {
      const Cell expected = materialize(machine, clause.head.args[index], env);
      const Cell actual = Cell{machine.heap.word_at(goal + kFrameHead + index)};
      if (!do_unify(machine, expected, actual)) {
        machine.failed = true;
        return;
      }
    }
  }
  const auto cont = machine.heap.word_at(goal + 3);
  launch(machine, clause.body, cont, cut, env);
}

void enter_user(Impl& machine, const Goal& goal) {
  Predicate* predicate = nullptr;
  for (Predicate& candidate : machine.predicates) {
    if (candidate.name == goal.name && candidate.arity == goal.arity) predicate = &candidate;
  }
  if (predicate == nullptr || predicate->clauses.empty()) {
    const Cell pi = cell_functor(machine, goal.name, goal.arity);
    raise_formal(machine, formal_existence(machine, "procedure", pi), goal.name, goal.arity);
    return;
  }
  const bool has_arg = goal.arity >= 1;
  const Cell first = has_arg ? machine.heap.deref(goal.arg(machine, 0)) : Cell{};
  const auto candidates = select_clauses(machine, *predicate, has_arg, first);
  if (candidates.empty()) {
    machine.failed = true;
    return;
  }
  const auto boundary = machine.stack.size();
  if (candidates.size() > 1) {
    Entry entry;
    entry.pick = Pick::Clauses;
    entry.candidates = candidates;
    entry.next = 1;
    entry.redo_goal = goal.at;
    entry.pred_name = goal.name;
    entry.pred_arity = goal.arity;
    entry.cut = boundary;
    entry.heap_mark = machine.heap.size();
    entry.trail_mark = machine.trail.slots.size();
    machine.stack.push_back(std::move(entry));
  }
  enter_clause(machine, predicate->clauses[candidates[0]], goal.at, boundary);
}

bool backtrack(Impl& machine) {
  machine.failed = false;
  machine.exception = false;
  while (!machine.stack.empty()) {
    const Entry top = machine.stack.back();
    if (top.pick == Pick::Catch || top.pick == Pick::Cleanup) {
      restore_state(machine, top.heap_mark, top.trail_mark);
      machine.stack.pop_back();
      continue;
    }
    restore_state(machine, top.heap_mark, top.trail_mark);
    if (top.pick == Pick::Clauses) {
      if (top.next >= top.candidates.size()) {
        machine.stack.pop_back();
        continue;
      }
      const auto index = top.candidates[top.next];
      const auto name = top.pred_name;
      const auto arity = top.pred_arity;
      const auto goal = top.redo_goal;
      const auto cut = top.cut;
      if (top.next + 1 >= top.candidates.size()) machine.stack.pop_back();
      else machine.stack.back().next += 1;
      const Predicate* predicate = nullptr;
      for (const Predicate& candidate : machine.predicates) {
        if (candidate.name == name && candidate.arity == arity) predicate = &candidate;
      }
      if (predicate == nullptr) continue;
      enter_clause(machine, predicate->clauses[index], goal, cut);
      return true;
    }
    if (top.pick == Pick::ElseGoal) {
      if (top.soft_id != 0 && top.cond_ok) {
        machine.stack.pop_back();
        continue;
      }
      const IrTerm goal = top.else_goal;
      const auto env = top.env;
      const auto cont = top.cont;
      const auto cut = top.cut;
      machine.stack.pop_back();
      launch(machine, goal, cont, cut, env);
      return true;
    }
    if (top.pick == Pick::Negation) {
      const auto cont = top.cont;
      machine.stack.pop_back();
      machine.ip = cont;
      return true;
    }
    if (top.pick == Pick::Once) {
      machine.stack.pop_back();
      continue;
    }
    if (top.pick == Pick::Repeat) {
      machine.ip = top.cont;
      return true;
    }
  }
  return false;
}

}
