// :/2 限定ゴール（SPEC 第5節、ADR 0006、iso-notes「Section 5 Cut inside Module:Goal」）。
// モード: :(+atom, +callable)。
// エラー: instantiation_error、type_error(atom, M)、existence_error(module, M)、type_error(callable, G)。
// テスト: tests/pl/modules.pl
#include "module.hpp"

#include "../machine/internal.hpp"

namespace mosa {

namespace {

std::uint32_t atom_of(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

bool is_callable(Cell cell) { return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis; }

}  // namespace

bool run_qualify(Impl& machine, const Goal& goal) {
  if (goal.arity != 2 || machine.atoms.name(goal.name) != ":") return false;
  const Cell module = machine.heap.deref(goal.arg(machine, 0));
  const Cell inner = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(module) || machine.heap.unbound(inner)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (module.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", module), goal.name, goal.arity);
    return true;
  }
  if (find_module(machine, atom_of(module)) == nullptr) {
    raise_formal(machine, formal_existence(machine, "module", module), goal.name, goal.arity);
    return true;
  }
  // TODO(iso): 7.8.3.3 の culprit は call/1 にならって G とした。M:G 全体にするかは未確認
  if (!is_callable(inner)) {
    raise_formal(machine, formal_type(machine, "callable", inner), goal.name, goal.arity);
    return true;
  }
  const Compiled compiled = compile_goal(machine.heap, inner);
  if (!body_callable(compiled.term)) {
    raise_formal(machine, formal_type(machine, "callable", inner), goal.name, goal.arity);
    return true;
  }
  // 枠のカットの境界をそのまま渡す。M:G はカットに透明である（iso-notes の決定 A）。
  launch(machine, compiled.term, goal.cont, goal.cut, compiled.env, atom_of(module));
  return true;
}

}
