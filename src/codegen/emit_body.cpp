// 計画 Task 7、ADR 0009。本体のゴールの並びを、call_goal と Ctrl::Native の継続枠で動かす。
// 1 つの関数は、再開の点から次の呼び出し（か節の終わり）までの直線の文を出す。呼び出しの継続は次の状態の
// 継続枠で、その状態の関数が枠から変数を読んで続きを出す。呼び先の C++ 関数は直に呼ばない。直に呼ぶと
// C++ のスタックが Prolog の深さで伸びる（ADR 0009）。制御構文の枝は emit_control.cpp が同じ仕組みで出す。
// 動かせない形（halt、数や匿名変数のゴール）を含む節は、呼び出し側（emit.cpp）が interpret_clause に回す。

#include "emit_state.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {
namespace {

void mark_slots(const IrTerm& term, std::vector<int>& counts) {
  if (term.tag == IrTag::Var) {
    if (term.slot < counts.size()) counts[term.slot] += 1;
    return;
  }
  for (const IrTerm& arg : term.args) mark_slots(arg, counts);
}

// goals が使う変数をスロットの順に。継続枠の語 5 から先にこの順で置き、状態の関数も同じ順で読む。
std::vector<std::uint16_t> live_slots(ClauseCtx& ctx, std::span<const GoalRef> goals, std::size_t vars) {
  std::vector<int> counts(vars, 0);
  count_body(ctx, goals, counts);
  std::vector<std::uint16_t> live;
  for (std::size_t slot = 0; slot < vars; ++slot) {
    if (counts[slot] > 0) live.push_back(static_cast<std::uint16_t>(slot));
  }
  return live;
}

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

// spec の状態の関数を出す。枠の変数は skip 個の整数セルのあとに、使う変数がスロットの順で並ぶ。
// 再開の状態ではカットの境界は継続枠の語 2 で、regs.cut は状態 0 と節の入口でだけ正しい（ADR 0009）。
std::vector<std::uint16_t> emit_state(ClauseCtx& ctx, const StateSpec& spec, std::uint32_t state) {
  const Clause& clause = ctx.predicate.clauses[ctx.name.clause - 1];
  ClauseEmit fn{ctx.machine, ctx.tables, {}, {}, 0, 1};
  fn.slots.assign(clause.vars, SlotState{});
  const auto live = live_slots(ctx, spec.goals, clause.vars);
  for (std::size_t index = 0; index < live.size(); ++index) {
    fn.line("const Cell " + var_name(live[index]) + " = rt::var(r, frame, " + std::to_string(spec.skip + index) + ");");
  }
  if (!spec.prologue.empty()) fn.line(spec.prologue);
  if (spec.fails) {
    fn.line("return rt::fail(m);");
  } else {
    BodyEnv env{"rt::frame_cut(r, frame)", "rt::frame_cont(r, frame)"};
    if (spec.local_cut) {
      fn.line("const std::size_t cut = rt::choice_mark(m);");
      env.cut = "cut";
    }
    emit_seq(ctx, fn, spec.goals, env);
  }
  const std::string function = state_function(ctx, state);
  ctx.code.functions += function_text(function, fn.out);
  ctx.code.cases.emplace_back(rt::state_of(ctx.name.clause, state), "return " + function + "(m, r, frame);");
  return live;
}

// ゴールを呼ぶ。続きがあれば次の状態の継続枠を作り、無ければ今の継続へ直に返す。
void emit_call(ClauseCtx& ctx, ClauseEmit& emit, const GoalRef& ref, std::span<const GoalRef> rest,
               const BodyEnv& env) {
  const IrTerm& goal = *ref.term;
  std::vector<std::string> parts;
  if (goal.tag != IrTag::Atom) {
    for (const IrTerm& arg : goal.args) parts.push_back(emit_build(emit, arg));
  }
  const std::string name{ctx.machine.atoms.name(goal.atom)};
  const std::string args = cell_list(emit, "g", parts);
  const std::string cont = make_rest(ctx, emit, rest, env);
  const std::size_t callee = emit.tables.call(ref.module, name, static_cast<std::uint16_t>(parts.size()));
  emit.line("rt::call_goal(m, r, " + std::to_string(callee) + ", " + args + ", " + cont + ", " + env.cut + ");");
}

}  // namespace

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

bool translatable_body(const Impl& machine, const IrTerm& body) {
  switch (body.tag) {
    case IrTag::True:
    case IrTag::Fail:
    case IrTag::Cut:
    case IrTag::Atom:
    case IrTag::Fun:
    case IrTag::Call: return true;
    case IrTag::And:
      return body.args.size() == 2 && translatable_body(machine, body.args[0]) &&
             translatable_body(machine, body.args[1]);
    case IrTag::Or:
      return body.args.size() == 2 && body.args[0].tag != IrTag::SoftIf && translatable_body(machine, body.args[0]) &&
             translatable_body(machine, body.args[1]);
    case IrTag::IfThen:
      return body.args.size() == 2 && translatable_body(machine, body.args[0]) &&
             translatable_body(machine, body.args[1]);
    default: return false;
  }
}

IrTerm goal_view(const Impl& machine, const IrTerm& data) {
  IrTerm goal;
  if (data.tag == IrTag::Atom) {
    const auto name = machine.atoms.name(data.atom);
    if (name == "true") goal.tag = IrTag::True;
    else if (name == "fail" || name == "false") goal.tag = IrTag::Fail;
    else if (name == "!") goal.tag = IrTag::Cut;
    else if (name == "halt") goal.tag = IrTag::Halt;
    else return data;
    return goal;
  }
  if (data.tag != IrTag::Fun) return data;
  const auto name = machine.atoms.name(data.atom);
  const std::size_t arity = data.args.size();
  if (arity == 2 && (name == "," || name == ";" || name == "->" || name == "*->")) {
    if (name == ",") goal.tag = IrTag::And;
    else if (name == ";") goal.tag = IrTag::Or;
    else if (name == "->") goal.tag = IrTag::IfThen;
    else goal.tag = IrTag::SoftIf;
    goal.args.push_back(goal_view(machine, data.args[0]));
    goal.args.push_back(goal_view(machine, data.args[1]));
  } else if (arity == 3 && name == "catch") {
    goal.tag = IrTag::Catch;
    goal.args.push_back(goal_view(machine, data.args[0]));
    goal.args.push_back(data.args[1]);
    goal.args.push_back(goal_view(machine, data.args[2]));
  } else if (arity == 1 && name == "throw") {
    goal.tag = IrTag::Throw;
    goal.args.push_back(data.args[0]);
  } else if (arity == 1 && name == "halt") {
    goal.tag = IrTag::Halt;
    goal.args.push_back(data.args[0]);
  } else {
    return data;
  }
  return goal;
}

void flatten(ClauseCtx& ctx, const IrTerm& term, std::uint32_t module, std::vector<GoalRef>& out) {
  // fail と throw のあとに届かないゴールは、変数を数える側も発出も見ない。見ると使わない変数を宣言してしまう。
  if (!out.empty() && (out.back().term->tag == IrTag::Fail || out.back().term->tag == IrTag::Throw)) return;
  if (term.tag == IrTag::True) return;
  if (term.tag == IrTag::And && term.args.size() == 2) {
    flatten(ctx, term.args[0], module, out);
    flatten(ctx, term.args[1], module, out);
    return;
  }
  out.push_back(GoalRef{&term, module});
}

void count_body(ClauseCtx& ctx, std::span<const GoalRef> goals, std::vector<int>& counts) {
  for (const GoalRef& ref : goals) {
    const IrTerm& goal = *ref.term;
    switch (goal.tag) {
      case IrTag::Fail:
      case IrTag::Cut: continue;
      case IrTag::Or: {
        // (A -> B ; C) と (A *-> B ; C) は 3 つの枝。then と else の枠は構文の入口で作るので、全部を数える。
        const IrTerm& left = goal.args[0];
        if (left.tag == IrTag::IfThen || left.tag == IrTag::SoftIf) {
          count_branch(ctx, left.args[0], ref.module, counts);
          count_branch(ctx, left.args[1], ref.module, counts);
        } else {
          count_branch(ctx, left, ref.module, counts);
        }
        count_branch(ctx, goal.args[1], ref.module, counts);
        continue;
      }
      case IrTag::IfThen:
      case IrTag::SoftIf:
        count_branch(ctx, goal.args[0], ref.module, counts);
        count_branch(ctx, goal.args[1], ref.module, counts);
        continue;
      default: mark_slots(goal, counts);
    }
  }
}

std::string make_rest(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> rest, const BodyEnv& env) {
  if (rest.empty()) return env.cont;
  StateSpec spec;
  spec.goals.assign(rest.begin(), rest.end());
  return make_frame(ctx, emit, "next", spec, env.cut, env.cont, {});
}

std::string make_frame(ClauseCtx& ctx, ClauseEmit& emit, std::string_view stem, const StateSpec& spec,
                       const std::string& cut, const std::string& cont, const std::vector<std::string>& prefix) {
  const std::uint32_t state = ctx.next_state++;
  std::vector<std::string> cells = prefix;
  for (const std::uint16_t slot : emit_state(ctx, spec, state)) cells.push_back(var_name(slot));
  const std::string kept = cell_list(emit, "k", cells);
  const std::string frame = emit.fresh(stem);
  emit.line("const auto " + frame + " = rt::resume_frame(m, &" + ctx.name.proc + ", " +
            state_text(ctx.name.clause, state) + ", " + cut + ", " + cont + ", " + kept + ");");
  return frame;
}

std::string mark_cell(std::string_view value) {
  return "rt::small_int(static_cast<std::int64_t>(" + std::string{value} + "))";
}

// goals を順に出す。呼び出しと構文は続きまで出し切って戻るので、ここに戻るのは直線の文（カットなど）のあとだけ。
void emit_seq(ClauseCtx& ctx, ClauseEmit& emit, std::span<const GoalRef> goals, const BodyEnv& env) {
  for (std::size_t index = 0; index < goals.size(); ++index) {
    const GoalRef& ref = goals[index];
    const auto rest = goals.subspan(index + 1);
    switch (ref.term->tag) {
      case IrTag::Fail: emit.line("return rt::fail(m);"); return;
      case IrTag::Cut: emit.line("rt::cut_to(m, " + env.cut + ");"); continue;
      case IrTag::Or: emit_or(ctx, emit, ref, rest, env); return;
      case IrTag::IfThen: emit_if_then(ctx, emit, ref, rest, env); return;
      default: break;
    }
    emit_call(ctx, emit, ref, rest, env);
    return;
  }
  emit.line("rt::proceed(m, " + env.cont + ");");
}

ClauseCode emit_clause(const Impl& machine, UnitTables& tables, const Predicate& predicate, const ClauseName& name) {
  ClauseCode code;
  const Clause& clause = predicate.clauses[name.clause - 1];
  ClauseCtx ctx{machine, tables, predicate, name, code, 1, {}};
  std::vector<GoalRef> goals;
  flatten(ctx, clause.body, predicate.module, goals);
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
  emit_head(emit, clause.head);
  emit_seq(ctx, emit, goals, BodyEnv{"r.cut", "rt::frame_cont(r, frame)"});
  const std::string entry = name.proc + "_c" + std::to_string(name.clause);
  code.functions += function_text(entry, emit.out);
  code.cases.emplace_back(entry_state, "return " + entry + "(m, r, frame);");
  return code;
}

}  // namespace mosa::codegen
