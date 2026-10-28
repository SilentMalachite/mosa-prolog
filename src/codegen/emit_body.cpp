// 計画 Task 4、ADR 0009。本体の Atom、Fun、Call の連言を、call_goal と Ctrl::Native の継続枠で動かす。
// 1 つ目のゴールは節の入口の関数から呼び、g 番目のゴールのあとは継続枠の状態 state_of(節, g) で再開する。
// 呼び先の C++ 関数は直に呼ばない。直に呼ぶと C++ のスタックが Prolog の深さで伸びる（ADR 0009）。
// それ以外の構文（カット、;、->、*->、catch/3、throw/1、halt、変数のゴール、リストのゴール）を含む節は
// 呼び出し側（emit.cpp）が interpret_clause に回す。Task 7 で減らす。

#include "emit_state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mosa::codegen {
namespace {

// 連言を呼ぶ順に並べる。true は飛ばし、fail のあとは届かないので見ない。
bool flatten(const IrTerm& term, BodyPlan& plan) {
  if (plan.fails) return true;
  switch (term.tag) {
    case IrTag::True: return true;
    case IrTag::Fail: plan.fails = true; return true;
    case IrTag::And: return term.args.size() == 2 && flatten(term.args[0], plan) && flatten(term.args[1], plan);
    case IrTag::Atom:
    case IrTag::Fun:
    case IrTag::Call: plan.goals.push_back(&term); return true;
    default: return false;
  }
}

void mark_slots(const IrTerm& term, std::vector<bool>& used) {
  if (term.tag == IrTag::Var) {
    if (term.slot < used.size()) used[term.slot] = true;
    return;
  }
  for (const IrTerm& arg : term.args) mark_slots(arg, used);
}

// from 番目以降のゴールが使う変数。継続枠の語 5 から先にスロットの順で置く。
std::vector<std::uint16_t> live_from(const BodyPlan& plan, std::size_t from, std::size_t vars) {
  std::vector<bool> used(vars, false);
  for (std::size_t index = from; index < plan.goals.size(); ++index) mark_slots(*plan.goals[index], used);
  std::vector<std::uint16_t> live;
  for (std::size_t slot = 0; slot < vars; ++slot) {
    if (used[slot]) live.push_back(static_cast<std::uint16_t>(slot));
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

// index 番目のゴールを呼ぶ。最後のゴールは節の継続へ直に返し、それ以外は次の状態の継続枠を作る。
// 再開の状態ではカットの境界は継続枠の語 2 で、regs.cut は状態 0 と節の入口でだけ正しい。
void emit_step(ClauseEmit& emit, const Predicate& predicate, const ClauseName& name, const BodyPlan& plan,
               std::size_t index, std::size_t vars) {
  if (index == plan.goals.size()) {
    emit.line("rt::proceed(m, rt::frame_cont(r, frame));");
    return;
  }
  const IrTerm& goal = *plan.goals[index];
  const std::string cut = index == 0 ? "r.cut" : "rt::frame_cut(r, frame)";
  std::vector<std::string> parts;
  if (goal.tag != IrTag::Atom) {
    for (const IrTerm& arg : goal.args) parts.push_back(emit_build(emit, arg));
  }
  const std::string args = cell_list(emit, "g", parts);
  std::string cont = "rt::frame_cont(r, frame)";
  if (index + 1 < plan.goals.size() || plan.fails) {
    std::vector<std::string> kept;
    for (const std::uint16_t slot : live_from(plan, index + 1, vars)) kept.push_back(var_name(slot));
    const std::string keep = cell_list(emit, "k", kept);
    cont = emit.fresh("next");
    emit.line("const auto " + cont + " = rt::resume_frame(m, &" + name.proc + ", " +
              state_text(name.clause, static_cast<std::uint32_t>(index + 1)) + ", " + cut +
              ", rt::frame_cont(r, frame), " + keep + ");");
  }
  const auto arity = static_cast<std::uint16_t>(goal.tag == IrTag::Atom ? 0 : goal.args.size());
  const std::size_t callee = emit.tables.call(PredKey{predicate.module, goal.atom, arity});
  emit.line("rt::call_goal(m, r, " + std::to_string(callee) + ", " + args + ", " + cont + ", " + cut + ");");
}

std::string function_text(const std::string& name, const std::string& body) {
  return "void " + name + "(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {\n" + body + "}\n\n";
}

std::string resume_name(const ClauseName& name, std::size_t goal) {
  return name.proc + "_c" + std::to_string(name.clause) + "_" + std::to_string(goal);
}

// g 番目のゴールのあとの再開。継続枠から変数を読み、次のゴールを呼ぶ。
void emit_resume(const Impl& machine, UnitTables& tables, const Predicate& predicate, const ClauseName& name,
                 const BodyPlan& plan, std::size_t goal, ClauseCode& code) {
  const Clause& clause = predicate.clauses[name.clause - 1];
  ClauseEmit emit{machine, tables, {}, {}, 0, 1};
  emit.slots.assign(clause.vars, SlotState{});
  const auto kept = live_from(plan, goal, clause.vars);
  for (std::size_t index = 0; index < kept.size(); ++index) {
    emit.line("const Cell " + var_name(kept[index]) + " = rt::var(r, frame, " + std::to_string(index) + ");");
  }
  emit_step(emit, predicate, name, plan, goal, clause.vars);
  const std::string function = resume_name(name, goal);
  code.functions += function_text(function, emit.out);
  code.cases.emplace_back(rt::state_of(name.clause, static_cast<std::uint32_t>(goal)),
                          "return " + function + "(m, r, frame);");
}

}  // namespace

std::optional<BodyPlan> plan_body(const IrTerm& body) {
  BodyPlan plan;
  if (!flatten(body, plan)) return std::nullopt;
  return plan;
}

ClauseCode emit_clause(const Impl& machine, UnitTables& tables, const Predicate& predicate,
                       const ClauseName& name, const BodyPlan& plan) {
  ClauseCode code;
  const auto entry_state = rt::state_of(name.clause, 0);
  // 頭部の単一化は束縛を残さずに失敗するか、本体の fail で戻るだけなので、本体がすぐ失敗する節は頭部を見ない。
  if (plan.goals.empty() && plan.fails) {
    code.cases.emplace_back(entry_state, "return rt::fail(m);");
    return code;
  }
  const Clause& clause = predicate.clauses[name.clause - 1];
  ClauseEmit emit{machine, tables, {}, {}, 0, 1};
  plan_slots(clause, plan, emit);
  emit_slot_cells(emit);
  emit_head(emit, clause.head);
  emit_step(emit, predicate, name, plan, 0, clause.vars);
  const std::string entry = name.proc + "_c" + std::to_string(name.clause);
  code.functions += function_text(entry, emit.out);
  code.cases.emplace_back(entry_state, "return " + entry + "(m, r, frame);");
  for (std::size_t goal = 1; goal < plan.goals.size(); ++goal) {
    emit_resume(machine, tables, predicate, name, plan, goal, code);
  }
  // fail で終わる本体は、最後のゴールのあとの状態で失敗する。
  if (plan.fails) {
    code.cases.emplace_back(rt::state_of(name.clause, static_cast<std::uint32_t>(plan.goals.size())),
                            "return rt::fail(m);");
  }
  return code;
}

}  // namespace mosa::codegen
