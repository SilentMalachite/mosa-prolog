// docs/SPEC.md 第5節
// current_module(?atom)
//   type_error(atom, Culprit)
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

namespace mosa {
namespace {

constexpr int kModuleGen = 6;

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

void push_module(Impl& machine, const Goal& goal, std::size_t next, std::size_t heap_mark,
                 std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kModuleGen;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool prove_modules(Impl& machine, const Goal& goal, std::size_t begin) {
  for (std::size_t index = begin; index < machine.modules.size(); ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool more = index + 1 < machine.modules.size();
    if (more) push_module(machine, goal, index + 1, heap_mark, trail_mark);
    const Cell name = machine.heap.atom(machine.modules[index]);
    if (do_unify(machine, goal.arg(machine, 0), name)) {
      succeed(machine, goal);
      return true;
    }
    if (more) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return false;
}

bool known_module(const Impl& machine, Cell arg) {
  const auto name = static_cast<std::uint32_t>(arg.raw() >> 3);
  for (const std::uint32_t existing : machine.modules) {
    if (existing == name) return true;
  }
  return false;
}

}  // namespace

bool redo_module_gen(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  machine.stack.pop_back();
  return prove_modules(machine, goal, top.next);
}

bool run_modules(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "current_module" || goal.arity != 1) return false;
  const Cell arg = machine.heap.deref(goal.arg(machine, 0));
  // 未束縛は列挙。型より先に見るので、変数を type_error にしない。
  if (machine.heap.unbound(arg)) {
    if (!prove_modules(machine, goal, 0)) machine.failed = true;
    return true;
  }
  if (arg.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", arg), goal.name, goal.arity);
    return true;
  }
  if (known_module(machine, arg)) succeed(machine, goal);
  else machine.failed = true;
  return true;
}

}  // namespace mosa
