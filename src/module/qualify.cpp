// Module:Goal。13211-2 の構文。意味は docs/SPEC.md 第5節（述語ベースのモジュール）。
// :(+atom, +callable)
//   instantiation_error, type_error(atom), existence_error(module), type_error(callable)
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

namespace mosa {

bool run_qualify(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != ":" || goal.arity != 2) return false;
  const Cell module = machine.heap.deref(goal.arg(machine, 0));
  // 変数は型より先。未束縛を type_error にしない。
  if (machine.heap.unbound(module)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (module.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", module), goal.name, goal.arity);
    return true;
  }
  const auto home = static_cast<std::uint32_t>(module.raw() >> 3);
  if (!module_loaded(machine, home)) {
    raise_formal(machine, formal_existence(machine, "module", module), goal.name, goal.arity);
    return true;
  }
  const Cell inner = machine.heap.deref(goal.arg(machine, 1));
  const Compiled compiled = compile_goal(machine.heap, inner);
  if (!body_callable(compiled.term)) {
    raise_formal(machine, formal_type(machine, "callable", inner), goal.name, goal.arity);
    return true;
  }
  const auto caller = machine.calling;
  // ゴールの探索は名付けたモジュール。成功も失敗も呼び出し元へ戻す。
  machine.calling = home;
  auto cont = goal.cont;
  if (caller != home) {
    arm_caller(machine, caller);
    cont = restore_calling(machine, caller, cont);
  }
  launch(machine, compiled.term, cont, machine.stack.size(), compiled.env);
  return true;
}

}  // namespace mosa
