// 計画 Task 7、ADR 0009。本体のゴールの並びを、call_goal と Ctrl::Native の継続枠で動かす。
// 1 つの関数は、再開の点から次の呼び出し（か節の終わり）までの直線の文を出す。呼び出しの継続は次の状態の
// 継続枠で、その状態の関数が枠から変数を読んで続きを出す。呼び先の C++ 関数は直に呼ばない。直に呼ぶと
// C++ のスタックが Prolog の深さで伸びる（ADR 0009）。制御構文の枝は emit_control.cpp が同じ仕組みで出す。
// 動かせない形（halt、数や匿名変数のゴール）を含む節は、呼び出し側（emit.cpp）が interpret_clause に回す。

#include "emit_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa::codegen {
namespace {

std::string cell_list(ClauseEmit& emit, std::string_view stem, const std::vector<std::string>& items) {
  if (items.empty()) return "{}";
  std::string joined;
  for (const std::string& item : items) joined += (joined.empty() ? "" : ", ") + item;
  const std::string name = emit.fresh(stem);
  emit.line("const std::array<Cell, " + std::to_string(items.size()) + "> " + name + "{" + joined + "};");
  return name;
}

bool identifier_unit(char unit) {
  return unit == '_' || (unit >= '0' && unit <= '9') || (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z');
}

// fail や throw だけの状態は r や frame を読まないので、使わない引数は名前を伏せる（-Wunused-parameter を誤りにしてある）。
std::string function_text(const std::string& name, const std::string& body) {
  const std::string regs = uses_identifier(body, "r") ? "const rt::Regs& r" : "const rt::Regs& /*r*/";
  const std::string frame = uses_identifier(body, "frame") ? "std::uint64_t frame" : "std::uint64_t /*frame*/";
  return "void " + name + "(Machine::Impl& m, " + regs + ", " + frame + ") {\n" + body + "}\n\n";
}

std::string state_function(const ClauseCtx& ctx, std::uint32_t state) {
  return ctx.name.proc + "_c" + std::to_string(ctx.name.clause) + "_s" + std::to_string(state);
}

// spec の状態の関数を出す。枠の変数は skip 個の整数セルのあとに、使う変数 live がスロットの順で並ぶ（継続枠の語 5 から
// 先にこの順で置く）。再開の状態ではカットの境界は継続枠の語 2 で、regs.cut は状態 0 と節の入口でだけ正しい（ADR 0009）。
// 待ち行列から呼ぶ（emit_clause）。中で作る状態はまた待ち行列に入るので、C++ の呼び出しは状態の連なりで重ならない。
void emit_state(ClauseCtx& ctx, const StateSpec& spec, std::uint32_t state, const std::vector<std::uint32_t>& live) {
  const Clause& clause = ctx.predicate.clauses[ctx.name.clause - 1];
  ClauseEmit fn{ctx.machine, ctx.tables, {}, {}, 0, 1};
  fn.slots.assign(clause.vars, SlotState{});
  for (std::size_t index = 0; index < live.size(); ++index) {
    fn.line("const Cell " + var_name(live[index]) + " = rt::var(r, frame, " + std::to_string(spec.skip + index) + ");");
  }
  if (!spec.prologue.empty()) fn.line(spec.prologue);
  if (spec.fails) {
    fn.line("return rt::fail(m);");
  } else {
    BodyEnv env{"rt::frame_cut(r, frame)", "rt::frame_cont(r, frame)"};
    const std::size_t at = fn.out.size();
    if (spec.local_cut) {
      env.cut = fn.fresh("cut");
      fn.line("const std::size_t " + env.cut + " = rt::choice_mark(m);");
    }
    const std::size_t after = fn.out.size();
    if (spec.prologue_cuts) yield_after_cut(ctx, fn, spec.goals, env);
    emit_seq(ctx, fn, spec.goals, env);
    // 回復ゴールがカットも呼び出しも持たなければ境界を読まない。宣言だけ残すと未使用の警告になる。
    if (spec.local_cut && !uses_identifier(std::string_view{fn.out}.substr(after), env.cut)) fn.out.erase(at, after - at);
  }
  const std::string function = state_function(ctx, state);
  ctx.code.functions += function_text(function, fn.out);
  ctx.code.cases.emplace_back(rt::state_of(ctx.name.clause, state), "return " + function + "(m, r, frame);");
}

// ゴールを呼ぶ。続きがあれば次の状態の継続枠を作り、無ければ今の継続へ直に返す。
void emit_call(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& ref, std::span<const GoalRef> rest,
               const BodyEnv& env) {
  const IrTerm& goal = *ref.term;
  std::vector<std::string> parts;
  std::string name;
  if (goal.tag == IrTag::Var) {
    // 変数のゴールは call(G) と同じで、中のカットは呼び出しに閉じる（7.8.3、launch_var）。組み込みの call/1 に渡す。
    name = "call";
    parts.push_back(emit_build(emit, goal));
  } else {
    // リストのゴールは '.'/2 である（launch_list）。
    name = goal.tag == IrTag::List ? std::string{"."} : std::string{ctx.machine.atoms.name(goal.atom)};
    if (goal.tag != IrTag::Atom) {
      for (const IrTerm& arg : goal.args) parts.push_back(emit_build(emit, arg));
    }
  }
  const std::string args = cell_list(emit, "g", parts);
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::size_t callee = emit.tables.call(ref.module, name, static_cast<std::uint16_t>(parts.size()));
  emit.line("rt::call_goal(m, r, " + std::to_string(callee) + ", " + args + ", " + cont + ", " + env.cut + ");");
}

// is/2 と算術比較なら rt::Compare の名前（is は "is"）。他は nullopt。
std::optional<std::string_view> arith_op(const Impl& machine, const IrTerm& goal) {
  static constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kOps{
      {{"is", "is"}, {"=:=", "Eq"}, {"=\\=", "Ne"}, {"<", "Lt"}, {"=<", "Le"}, {">", "Gt"}, {">=", "Ge"}}};
  for (const auto& [name, op] : kOps) {
    if (is_name(machine, goal, name, 2)) return op;
  }
  return std::nullopt;
}

// is/2 と算術比較は組み込みの C++ を直に呼び、同じ関数で続きを進める（計画 Task 7）。失敗と例外は機械に記してあるので、
// false なら戻るだけ。組み込みは run_builtin が先に取るので、どのモジュールでも利用者の同名の述語には行かない。
// 述語のアトムは誤りの文脈のために単位の表から渡す（呼び出しのたびに名前を引かない）。
void emit_arith(ClauseEmit& emit, const IrTerm& goal, std::string_view op) {
  const std::string left = emit_build(emit, goal.args[0]);
  const std::string right = emit_build(emit, goal.args[1]);
  const std::string name = "r.atoms[" + std::to_string(emit.tables.atom(goal.atom)) + "]";
  if (op == "is") {
    emit.line("if (!rt::arith_is(m, " + name + ", " + left + ", " + right + ")) return;");
    return;
  }
  emit.line("if (!rt::arith_compare(m, rt::Compare::" + std::string{op} + ", " + name + ", " + left + ", " + right +
            ")) return;");
}

}  // namespace

bool arith_goal(const Impl& machine, const IrTerm& goal) { return arith_op(machine, goal).has_value(); }

bool uses_identifier(std::string_view text, std::string_view ident) {
  std::size_t at = 0;
  while (at < text.size()) {
    if (!identifier_unit(text[at])) {
      at += 1;
      continue;
    }
    const std::size_t start = at;
    while (at < text.size() && identifier_unit(text[at])) at += 1;
    if (text.substr(start, at - start) == ident) return true;
  }
  return false;
}

std::string make_rest(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> rest, const BodyEnv& env) {
  if (rest.empty()) return env.cont;
  StateSpec spec;
  spec.goals = rest;
  return make_frame(ctx, emit, "next", spec, env.cut, env.cont, {});
}

namespace {

// spec の状態の番号と、枠が運ぶ変数。仕様の全体が同じ状態は 1 つを使い回す（ADR 0011 の改訂 5）。状態の関数は仕様だけで
// 決まり、並びは ctx.lists の中の接尾なので、先頭の番地と長さで比べられる。カットの後で後始末へ譲る枠
// （yield_after_cut）はその場で続ける道と同じ続きを指し、入れ子の -> の then はそこへ至る道ごとに作られるので、
// まとめないと状態が段の数の 2 乗に増え、どれも部分木を数えて 3 乗の時間になった（H-30）。関数は待ち行列から出す。
std::pair<std::uint32_t, std::vector<std::uint32_t>> state_of_spec(ClauseCtx& ctx, const StateSpec& spec) {
  auto& by_rest = ctx.states[spec.goals.data()];
  StateRest rest{spec.goals.size(), spec.skip, spec.prologue, spec.local_cut, spec.fails, spec.prologue_cuts};
  if (const auto found = by_rest.find(rest); found != by_rest.end()) return found->second;
  const std::uint32_t state = ctx.next_state++;
  std::pair<std::uint32_t, std::vector<std::uint32_t>> made{state, live_of(ctx, spec.goals)};
  ctx.queue.push_back(PendingState{spec, state, made.second});
  by_rest.emplace(std::move(rest), made);
  return made;
}

}  // namespace

std::string make_frame(ClauseCtx& ctx, ClauseEmit& emit, std::string_view stem, const StateSpec& spec,
                       const std::string& cut, const std::string& cont, const std::vector<std::string>& prefix) {
  const auto [state, live] = state_of_spec(ctx, spec);
  std::vector<std::string> cells = prefix;
  for (const std::uint32_t slot : live) cells.push_back(var_name(slot));
  const std::string kept = cell_list(emit, "k", cells);
  const std::string frame = emit.fresh(stem);
  emit.line("const auto " + frame + " = rt::resume_frame(m, &" + ctx.name.proc + ", " +
            state_text(ctx.name.clause, state) + ", " + cut + ", " + cont + ", " + kept + ");");
  return frame;
}

std::string mark_cell(std::string_view value) {
  return "rt::small_int(static_cast<std::int64_t>(" + std::string{value} + "))";
}

namespace {

// rest がこの関数の中で動く文（算術、構文の入口と条件、\+、throw/1、次のカット）で始まるか。呼び出しと fail は
// 歩みを終えるだけで、後始末より先に束縛を読みも作りもしない。throw/1 は投げる項が変数かをその場で見るので、
// 後始末の束縛を見るには回す（Codex レビュー）。巻き戻しは propagate_throw が後始末の後へ回す。
bool runs_inline(const ClauseCtx& ctx, std::span<const GoalRef> rest) {
  if (rest.empty()) return false;
  const IrTerm& goal = *rest.front().term;
  switch (goal.tag) {
    case IrTag::Fail: return false;
    case IrTag::Throw:
    case IrTag::Cut:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
    case IrTag::Catch: return true;
    default: break;
  }
  return arith_op(ctx.machine, goal).has_value() || is_name(ctx.machine, goal, "\\+", 1);
}

}  // namespace

// カットが setup_call_cleanup/3 の後始末を外したら、rest を次の歩みへ回す。後始末は run_loop が歩みの間で動かすので、
// 回さないと rest のうちこの関数の中で動く文が先に動き、インタプリタと違う束縛を後始末が見る（ADR 0016、敵対的
// レビュー A4）。外していなければ、そのままこの関数で続ける。
void yield_after_cut(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> rest, const BodyEnv& env) {
  if (!runs_inline(ctx, rest)) return;
  emit.line("if (rt::cleanup_pending(m)) {");
  const std::string frame = make_rest(ctx, emit, rest, env);
  emit.line("return rt::proceed(m, " + frame + ");");
  emit.line("}");
}

// goals を順に出す。呼び出しと構文は続きまで出し切って戻るので、ここに戻るのは直線の文（カットなど）のあとだけ。
void emit_seq(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> goals, const BodyEnv& env) {
  for (std::size_t index = 0; index < goals.size(); ++index) {
    const GoalRef& ref = goals[index];
    const auto rest = goals.subspan(index + 1);
    switch (ref.term->tag) {
      case IrTag::Fail: emit.line("return rt::fail(m);"); return;
      case IrTag::Throw: emit.line("return rt::throw_term(m, " + emit_build(emit, ref.term->args[0]) + ");"); return;
      case IrTag::Cut:
        emit.line("rt::cut_to(m, " + env.cut + ");");
        yield_after_cut(ctx, emit, rest, env);
        continue;
      case IrTag::Or: emit_or(ctx, emit, ref, rest, env); return;
      case IrTag::IfThen: emit_if_then(ctx, emit, ref, rest, env); return;
      case IrTag::SoftIf: emit_soft(ctx, emit, ref, rest, env); return;
      case IrTag::Catch: emit_catch(ctx, emit, ref, rest, env); return;
      default: break;
    }
    if (const auto op = arith_op(ctx.machine, *ref.term); op.has_value()) {
      emit_arith(emit, *ref.term, *op);
      continue;
    }
    if (const IrTerm* inner = negated_goal(ctx, *ref.term); inner != nullptr) {
      emit_not(ctx, emit, *inner, ref.module, rest, env);
      return;
    }
    emit_call(ctx, emit, ref, rest, env);
    return;
  }
  emit.line("rt::proceed(m, " + env.cont + ");");
}

ClauseCode emit_clause(const Impl& machine, UnitTables& tables, const Predicate& predicate, const ClauseName& name) {
  ClauseCode code;
  const Clause& clause = predicate.clauses[name.clause - 1];
  ClauseCtx ctx{machine, tables, predicate, name, code};
  const std::span<const GoalRef> goals = flat_list(ctx, *clause.body, predicate.module);
  const auto entry_state = rt::state_of(name.clause, 0);
  // 頭部の単一化は束縛を残さずに失敗するか、本体の fail で戻るだけなので、本体がすぐ失敗する節は頭部を見ない。
  if (goals.size() == 1 && goals[0].term->tag == IrTag::Fail) {
    code.cases.emplace_back(entry_state, "return rt::fail(m);");
    return code;
  }
  std::vector<int> counts(clause.vars, 0);
  count_body(ctx, goals, counts);
  ClauseEmit emit{machine, tables, {}, {}, 0, 1};
  plan_slots(clause, counts, emit);
  emit_slot_cells(emit);
  emit_head(emit, clause.head, predicate);
  emit_seq(ctx, emit, goals, BodyEnv{"r.cut", "rt::frame_cont(r, frame)"});
  const std::string entry = name.proc + "_c" + std::to_string(name.clause);
  code.functions += function_text(entry, emit.out);
  code.cases.emplace_back(entry_state, "return " + entry + "(m, r, frame);");
  // 状態は作った順に出す。出す間に作った状態も後ろに並ぶ。番号が 16 ビットに入らなくなった節と、継続枠の変数が
  // kFrameCells を超えた節は呼び手が生成コードを捨てるので、そこで止める（捨てる状態の関数を書き続けない）。
  const auto oversized = [&ctx] { return ctx.oversized || ctx.next_state > kStateField + 1; };
  while (!ctx.queue.empty() && !oversized()) {
    const PendingState item = std::move(ctx.queue.front());
    ctx.queue.pop_front();
    emit_state(ctx, item.spec, item.state, item.live);
  }
  code.next_state = ctx.next_state;
  code.oversized = oversized();
  return code;
}

}  // namespace mosa::codegen
