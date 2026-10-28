// :/2 限定ゴール（SPEC 第5節、ADR 0006、iso-notes「Section 5 Cut inside Module:Goal」）。
// モード: :(+atom, +callable)。
// エラー: instantiation_error、type_error(atom, M)、existence_error(module, M)、type_error(callable, G)。
// テスト: tests/pl/modules.pl
#include "module.hpp"

#include "../machine/internal.hpp"

#include <array>

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
  const Compiled compiled = is_callable(inner) ? compile_goal(machine.heap, inner) : Compiled{};
  if (!is_callable(inner) || !body_callable(compiled.term)) {
    // culprit は利用者が書いた M:G 全体。call(M:G) の引数と同じ項になる（iso-notes の決定 A）。
    const std::array<Cell, 2> parts{module, inner};
    const Cell culprit = machine.heap.structure(goal.name, parts);
    raise_formal(machine, formal_type(machine, "callable", culprit), goal.name, goal.arity);
    return true;
  }
  // 枠のカットの境界をそのまま渡す。M:G はカットに透明である（iso-notes の決定 A）。
  launch(machine, compiled.term, goal.cont, goal.cut, compiled.env, atom_of(module));
  return true;
}

}
