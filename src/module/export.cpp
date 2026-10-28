// docs/SPEC.md 第5節
// export(+predicate_indicator)
//   instantiation_error, type_error(predicate_indicator), type_error(atom),
//   type_error(integer), domain_error(not_less_than_zero), representation_error(max_arity)
//   呼び出し中のモジュールの公開リストへ足す。既に取り込んだ側は更新しない。
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

namespace mosa {
namespace {

bool take_indicator(Impl& machine, const Goal& goal, Cell spec, std::uint32_t& name, std::uint16_t& arity) {
  const Cell left = machine.heap.deref(machine.heap.arg(spec, 0));
  const Cell right = machine.heap.deref(machine.heap.arg(spec, 1));
  if (machine.heap.unbound(left) || machine.heap.unbound(right)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return false;
  }
  if (left.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", left), goal.name, goal.arity);
    return false;
  }
  if (right.tag() != Tag::Int && right.tag() != Tag::IntBox) {
    raise_formal(machine, formal_type(machine, "integer", right), goal.name, goal.arity);
    return false;
  }
  const auto value = machine.heap.integer_value(right);
  if (value < 0) {
    raise_formal(machine, formal_domain(machine, "not_less_than_zero", right), goal.name, goal.arity);
    return false;
  }
  if (value > 1024) {
    raise_formal(machine, formal_representation(machine, "max_arity"), goal.name, goal.arity);
    return false;
  }
  name = static_cast<std::uint32_t>(left.raw() >> 3);
  arity = static_cast<std::uint16_t>(value);
  return true;
}

}  // namespace

void note_export(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  for (const ExportMark& item : machine.exports) {
    if (item.module == module && item.name == name && item.arity == arity) return;
  }
  machine.exports.push_back(ExportMark{module, name, arity});
}

bool run_export(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "export" || goal.arity != 1) return false;
  const Cell spec = machine.heap.deref(goal.arg(machine, 0));
  // 変数は型より先。未束縛を述語指示子の型誤りにしない。
  if (machine.heap.unbound(spec)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  const bool shaped = spec.tag() == Tag::Str && machine.heap.functor_arity(spec) == 2 &&
                      machine.atoms.name(machine.heap.functor_atom(spec)) == "/";
  if (!shaped) {
    raise_formal(machine, formal_type(machine, "predicate_indicator", spec), goal.name, goal.arity);
    return true;
  }
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (!take_indicator(machine, goal, spec, name, arity)) return true;
  // Module:export は呼び出しモジュールが Module になっている。
  note_export(machine, machine.calling, name, arity);
  machine.ip = goal.cont;
  return true;
}

}  // namespace mosa
