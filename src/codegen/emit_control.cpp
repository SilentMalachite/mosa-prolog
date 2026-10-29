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
  const std::string then = then_frame(ctx, emit, ite.args[1], module, env.cut, cont, mark);
  emit.line("rt::push_branch(m, " + other + ", " + env.cut + ");");
  emit_condition(ctx, emit, ite.args[0], module, then);
}

// (A *-> B ; C)。else を印の付く選択点で積み、A の成功で印を付けて B へ。A の選択点は残る（launch_or の SoftIf）。
void emit_soft_else(ClauseCtx& ctx, ClauseEmit& emit, const IrTerm& soft, const IrTerm& other_branch,
                    std::uint32_t module, std::span<const GoalRef> rest, const BodyEnv& env) {
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::string other = branch_frame(ctx, emit, "else", other_branch, module, env.cut, cont);
  const std::string id = emit.fresh("soft");
  emit.line("const std::size_t " + id + " = rt::push_soft(m, " + other + ", " + env.cut + ");");
  // then の枠だけは選択点のあとに作る。push_soft が返す印を運ぶからで、else の選択点が生きている間しか届かないので、
  // 戻って枠が消えても困らない。
  StateSpec spec;
  spec.goals = branch_goals(ctx, soft.args[1], module);
  spec.skip = 1;
  spec.prologue = "rt::soft_commit(m, static_cast<std::size_t>(rt::small_value(rt::var(r, frame, 0))));";
  const std::string then = make_frame(ctx, emit, "then", spec, env.cut, cont, {mark_cell(id)});
  emit_condition(ctx, emit, soft.args[0], module, then);
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
  if (left.tag == IrTag::SoftIf) return emit_soft_else(ctx, emit, left, goal.term->args[1], goal.module, rest, env);
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

// (A *-> B)。else が無いので印は要らない。A を局所のカット境界で動かし、成功のたびに B へ（launch_soft）。
void emit_soft(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
               const BodyEnv& env) {
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::string then = branch_frame(ctx, emit, "then", goal.term->args[1], goal.module, env.cut, cont);
  emit_condition(ctx, emit, goal.term->args[0], goal.module, then);
}

// \+ A。床（Pick::Negation）を積み、A を局所のカット境界で動かす。A が成功すれば床ごと切って失敗し（Ctrl::NegDone）、
// A が失敗して床へ戻れば続きへ進む（run_not、redo_not）。床は続きの枠より新しいので、戻っても枠は残る。
void emit_not(ClauseCtx& ctx, ClauseEmit& emit, const IrTerm& inner, std::uint32_t module,
              std::span<const GoalRef> rest, const BodyEnv& env) {
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::string mark = emit.fresh("mark");
  emit.line("const std::size_t " + mark + " = rt::choice_mark(m);");
  StateSpec spec;
  spec.skip = 1;
  spec.prologue = "rt::cut_to(m, static_cast<std::size_t>(rt::small_value(rt::var(r, frame, 0))));";
  spec.fails = true;
  const std::string done = make_frame(ctx, emit, "done", spec, env.cut, cont, {mark_cell(mark)});
  emit.line("rt::push_negation(m, " + cont + ");");
  emit_condition(ctx, emit, inner, module, done);
}

// catch(G, C, R)。R の継続枠と、G を抜けたあと Catch を無効にする枠を先に作り、Catch を積んでから G を局所のカット境界で
// 動かす（arm_catch）。例外は propagate_throw が Catch まで巻き戻し、ボールが C と単一化できれば R の枠へ進む。枠は
// Catch より古いので巻き戻しても残る。R の中のカットは R に閉じる（7.8.9）ので、R の状態は入口で境界を取る。
void emit_catch(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& goal, std::span<const GoalRef> rest,
                const BodyEnv& env) {
  const IrTerm& term = *goal.term;
  const std::string cont = make_rest(ctx, emit, rest, env);
  StateSpec recover;
  recover.goals = branch_goals(ctx, term.args[2], goal.module);
  recover.local_cut = true;
  const std::string recovery = make_frame(ctx, emit, "recover", recover, env.cut, cont, {});
  std::string exit = cont;
  // G が呼び出し無しに throw で終われば（catch(throw(x), ...)）抜けることがなく、枠を作っても使われない。
  if (dead_end(ctx, term.args[0], goal.module) == nullptr) {
    const std::string mark = emit.fresh("mark");
    emit.line("const std::size_t " + mark + " = rt::choice_mark(m);");
    StateSpec spec;
    spec.skip = 1;
    spec.prologue = "rt::catch_exit(m, static_cast<std::size_t>(rt::small_value(rt::var(r, frame, 0))));";
    exit = make_frame(ctx, emit, "exit", spec, env.cut, cont, {mark_cell(mark)});
  }
  const std::string pattern = emit_build(emit, term.args[1]);
  emit.line("rt::push_catch(m, " + pattern + ", " + recovery + ", " + env.cut + ");");
  emit_condition(ctx, emit, term.args[0], goal.module, exit);
}

}  // namespace mosa::codegen
