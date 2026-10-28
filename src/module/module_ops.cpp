// モジュールごとの演算子（SPEC 第5節、ADR 0006、iso-notes「Section 5 Operators in modules」）。
// 固有層、user 層（ISO の核を含む）の順に引く。公開リストの op(P, T, N) は取り込み側の層へ入る。
// エラー: op/3 と同じ（8.14.3）。instantiation_error、type_error(integer | atom | list)、
//         domain_error(operator_priority | operator_specifier)、permission_error(create | modify, operator, N)。
// テスト: tests/pl/modules.pl、tests/cpp/module_load_test.cpp
#include "module.hpp"

#include "../machine/internal.hpp"

namespace mosa {

OperatorView op_view(const Impl& machine, std::uint32_t module) {
  if (module == machine.user_module) return OperatorView{machine.operators};
  const Module* found = find_module(machine, module);
  if (found == nullptr) return OperatorView{machine.operators};
  return OperatorView{machine.operators, &found->ops};
}

OpResult define_op(Impl& machine, std::uint32_t module, int priority, Specifier specifier, std::string name) {
  Module* found = module == machine.user_module ? nullptr : find_module(machine, module);
  // user の op/3 はすべてのモジュールから見える。user の層は全体の表そのものである。
  if (found == nullptr) return machine.operators.define(priority, specifier, std::move(name));
  const OpResult result = OperatorView{machine.operators, &found->ops}.admits(priority, specifier, name);
  if (result != OpResult::Ok) return result;
  found->ops.define_local(priority, specifier, std::move(name));
  return OpResult::Ok;
}

std::optional<Cell> import_ops(Impl& machine, std::uint32_t from, std::uint32_t into) {
  const Module* source = find_module(machine, from);
  if (source == nullptr) return std::nullopt;
  // define_op は表を足すので、走査中の公開の並びを写してから入れる。
  const std::vector<OpDef> ops = source->export_ops;
  for (const OpDef& op : ops) {
    const OpResult result = define_op(machine, into, op.priority, op.specifier, op.name);
    if (result == OpResult::Ok) continue;
    const Cell culprit = machine.heap.atom(machine.atoms.intern(op.name));
    return formal_permission(machine, result == OpResult::ModifyComma ? "modify" : "create", "operator", culprit);
  }
  return std::nullopt;
}

}
