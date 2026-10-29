// 計画 Task 7、ADR 0009。制御構文を、インタプリタ（cut.cpp、control.cpp、catch.cpp）と同じ選択点を同じ順で
// 積む形で出す。枝の継続枠は構文より前に作る。選択点より古い枠は戻っても残るので、枝の状態の関数が枠から変数を
// 読める。構文のあとのゴールは 1 つの状態にまとめ、どの枝もその枠へ続く。

#include "emit_state.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {
namespace {

std::vector<GoalRef> branch_goals(ClauseCtx& ctx, const IrTerm& branch, std::uint32_t module) {
  std::vector<GoalRef> goals;
  flatten(ctx, branch, module, goals);
  return goals;
}

// 枝を動かす状態の継続枠。枝の変数を運び、終わったら cont へ続く。
std::string branch_frame(ClauseCtx& ctx, ClauseEmit& emit, std::string_view stem, const IrTerm& branch,
                         std::uint32_t module, const std::string& cut, const std::string& cont) {
  StateSpec spec;
  spec.goals = branch_goals(ctx, branch, module);
  return make_frame(ctx, emit, stem, spec, cut, cont, {});
}

}  // namespace

void count_branch(ClauseCtx& ctx, const IrTerm& branch, std::uint32_t module, std::vector<int>& counts) {
  const auto goals = branch_goals(ctx, branch, module);
  count_body(ctx, goals, counts);
}

// (A ; B)。else の枝 B を選択点に積んでから A を続ける（launch_or）。どちらの枝のカットも節の境界まで切る。
void emit_or(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
             const BodyEnv& env) {
  const IrTerm& left = goal.term->args[0];
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::string other = branch_frame(ctx, emit, "else", goal.term->args[1], goal.module, env.cut, cont);
  emit.line("rt::push_branch(m, " + other + ", " + env.cut + ");");
  const auto goals = branch_goals(ctx, left, goal.module);
  emit_seq(ctx, emit, goals, BodyEnv{env.cut, cont});
}

}  // namespace mosa::codegen
