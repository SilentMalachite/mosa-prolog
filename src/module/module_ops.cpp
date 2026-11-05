// モジュールごとの演算子（SPEC 第5節、ADR 0006、iso-notes「Section 5 Operators in modules」）。
// 固有層、user 層（ISO の核を含む）の順に引く。公開リストの op(P, T, N) は取り込み側の層へ入る。
// エラー: op/3 と同じ（8.14.3）。instantiation_error、type_error(integer | atom | list)、
//         domain_error(operator_priority | operator_specifier)、permission_error(create | modify, operator, N)。
// テスト: tests/pl/modules.pl、tests/cpp/module_load_test.cpp、tests/cpp/library_ops_test.cpp
#include "module.hpp"

#include "../machine/internal.hpp"

#include <array>
#include <utility>
#include <vector>

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
  if (found == nullptr) {
    const std::array<OpDef, 1> op{OpDef{priority, specifier, std::move(name)}};
    const auto failure = define_user_ops(machine, op);
    return failure ? failure->result : OpResult::Ok;
  }
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

std::optional<UserOpFailure> define_user_ops(Impl& machine, std::span<const OpDef> ops) {
  // 写しにすべて入れてから替える。退けた表だけが拒否する定義（利用者の表の後置と同じ名前の中置など）は、
  // パスで読むファイルと同じく permission_error になる（ADR 0006 の改訂 1、L19 A1 の Codex レビュー）。
  std::vector<OperatorTable> trials;
  trials.reserve(machine.set_aside_ops.size() + 1);
  trials.push_back(machine.operators);
  trials.insert(trials.end(), machine.set_aside_ops.begin(), machine.set_aside_ops.end());
  for (std::size_t index = 0; index < ops.size(); ++index) {
    for (OperatorTable& trial : trials) {
      const OpResult result = trial.define(ops[index].priority, ops[index].specifier, ops[index].name);
      if (result != OpResult::Ok) return UserOpFailure{index, result};
    }
  }
  machine.operators = std::move(trials.front());
  for (std::size_t index = 0; index < machine.set_aside_ops.size(); ++index) {
    machine.set_aside_ops[index] = std::move(trials[index + 1]);
  }
  return std::nullopt;
}

// 写しと ISO の表は作り終えてから入れ替える。作る途中で確保に失敗しても、利用者の表はまだ動いていない（L19 の Codex
// レビュー）。戻すときはムーブだけなので、デストラクタは投げない。
CoreOperatorScope::CoreOperatorScope(Impl& machine)
    : machine_(machine), saved_aside_(machine.set_aside_ops), saved_ops_(OperatorTable::iso_core()) {
  machine_.set_aside_ops.push_back(machine_.operators);
  std::swap(machine_.operators, saved_ops_);
}

CoreOperatorScope::~CoreOperatorScope() {
  if (restored_) return;
  machine_.operators = std::move(saved_ops_);
  machine_.set_aside_ops = std::move(saved_aside_);
}

// 退けた表は読み込みの間の user の op/3 を受けているので、そのまま戻す。核の表と見比べて写すと、ISO の既定と同じ値への
// 変更が消えた（L19 A1 の Codex レビュー）。
void CoreOperatorScope::finish() {
  if (restored_) return;
  machine_.operators = std::move(machine_.set_aside_ops.back());
  machine_.set_aside_ops.pop_back();
  restored_ = true;
}

}
