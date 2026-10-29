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

// 条件が成功したときの状態。枠の先頭の整数セルが切り戻す高さで、そこまで切ってから B を続ける（Ctrl::Commit）。
std::string then_frame(ClauseCtx& ctx, ClauseEmit& emit, const IrTerm& then, std::uint32_t module,
                       const std::string& cut, const std::string& cont, const std::string& mark) {
  StateSpec spec;
  spec.goals = branch_goals(ctx, then, module);
  spec.skip = 1;
  spec.prologue = "rt::cut_to(m, static_cast<std::size_t>(rt::small_value(rt::var(r, frame, 0))));";
  return make_frame(ctx, emit, "then", spec, cut, cont, {mark_cell(mark)});
}

// 条件 A を局所のカット境界で出す。成功すれば then の枠へ続く。条件の中のカットは else と外の選択点を捨てない（7.8.7）。
void emit_condition(ClauseCtx& ctx, ClauseEmit& emit, const IrTerm& cond, std::uint32_t module,
                    const std::string& then) {
  const std::string cut = emit.fresh("cut");
  const std::size_t at = emit.out.size();
  emit.line("const std::size_t " + cut + " = rt::choice_mark(m);");
  const std::size_t after = emit.out.size();
  const auto goals = branch_goals(ctx, cond, module);
  emit_seq(ctx, emit, goals, BodyEnv{cut, then});
  // 条件がカットも呼び出しも持たなければ（true など）境界を読まない。宣言だけ残すと未使用の警告になる。
  if (!uses_identifier(std::string_view{emit.out}.substr(after), cut)) emit.out.erase(at, after - at);
}

// (A -> B ; C)。積む前の高さを控え、else の枝 C を積む。A が成功すればその高さまで切って B へ（launch_or の IfThen）。
void emit_if_then_else(ClauseCtx& ctx, ClauseEmit& emit, const IrTerm& ite, const IrTerm& other_branch,
                       std::uint32_t module, std::span<const GoalRef> rest, const BodyEnv& env) {
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::string mark = emit.fresh("mark");
  emit.line("const std::size_t " + mark + " = rt::choice_mark(m);");
  const std::string other = branch_frame(ctx, emit, "else", other_branch, module, env.cut, cont);
  emit.line("rt::push_branch(m, " + other + ", " + env.cut + ");");
  const std::string then = then_frame(ctx, emit, ite.args[1], module, env.cut, cont, mark);
  emit_condition(ctx, emit, ite.args[0], module, then);
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
  if (left.tag == IrTag::IfThen) return emit_if_then_else(ctx, emit, left, goal.term->args[1], goal.module, rest, env);
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::string other = branch_frame(ctx, emit, "else", goal.term->args[1], goal.module, env.cut, cont);
  emit.line("rt::push_branch(m, " + other + ", " + env.cut + ");");
  const auto goals = branch_goals(ctx, left, goal.module);
  emit_seq(ctx, emit, goals, BodyEnv{env.cut, cont});
}

// (A -> B)。else が無いので、A を始める高さまで切って B へ（launch_if_then）。A が失敗すれば全体が失敗する。
void emit_if_then(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
                  const BodyEnv& env) {
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::string mark = emit.fresh("mark");
  emit.line("const std::size_t " + mark + " = rt::choice_mark(m);");
  const std::string then = then_frame(ctx, emit, goal.term->args[1], goal.module, env.cut, cont, mark);
  emit_condition(ctx, emit, goal.term->args[0], goal.module, then);
}

}  // namespace mosa::codegen
