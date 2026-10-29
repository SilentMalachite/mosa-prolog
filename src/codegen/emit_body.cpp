// 計画 Task 7、ADR 0009。本体のゴールの並びを、call_goal と Ctrl::Native の継続枠で動かす。
// 1 つの関数は、再開の点から次の呼び出し（か節の終わり）までの直線の文を出す。呼び出しの継続は次の状態の
// 継続枠で、その状態の関数が枠から変数を読んで続きを出す。呼び先の C++ 関数は直に呼ばない。直に呼ぶと
// C++ のスタックが Prolog の深さで伸びる（ADR 0009）。制御構文の枝は emit_control.cpp が同じ仕組みで出す。
// 動かせない形（halt、数や匿名変数のゴール）を含む節は、呼び出し側（emit.cpp）が interpret_clause に回す。

#include "emit_state.hpp"

#include "../module/module.hpp"

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
    const std::size_t at = fn.out.size();
    if (spec.local_cut) {
      env.cut = fn.fresh("cut");
      fn.line("const std::size_t " + env.cut + " = rt::choice_mark(m);");
    }
    const std::size_t after = fn.out.size();
    emit_seq(ctx, fn, spec.goals, env);
    // 回復ゴールがカットも呼び出しも持たなければ境界を読まない。宣言だけ残すと未使用の警告になる。
    if (spec.local_cut && !uses_identifier(std::string_view{fn.out}.substr(after), env.cut)) fn.out.erase(at, after - at);
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
    case IrTag::Var:
    case IrTag::List:
    case IrTag::Throw:
    case IrTag::Call: return true;
    case IrTag::And:
      return body.args.size() == 2 && translatable_body(machine, body.args[0]) &&
             translatable_body(machine, body.args[1]);
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
      return body.args.size() == 2 && translatable_body(machine, body.args[0]) &&
             translatable_body(machine, body.args[1]);
    case IrTag::Catch:
      return body.args.size() == 3 && translatable_body(machine, body.args[0]) &&
             translatable_body(machine, body.args[2]);
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

namespace {

// データの形で持つゴール（\+ の中、M:G の G）をゴールの形に直して ctx.owned に置く。変数や数や、(true, 1) のように
// 動かせないゴールを含む形は、呼び出しのときに組み込みが確かめる（instantiation_error、type_error）ので nullptr。
const IrTerm* owned_goal(ClauseCtx& ctx, const IrTerm& data) {
  if (data.tag != IrTag::Atom && data.tag != IrTag::Fun) return nullptr;
  IrTerm goal = goal_view(ctx.machine, data);
  if (!translatable_body(ctx.machine, goal)) return nullptr;
  ctx.owned.push_back(std::move(goal));
  return &ctx.owned.back();
}

// \+ の中。組み込みの \+/1 に任せる形は nullptr。
const IrTerm* negated_goal(ClauseCtx& ctx, const IrTerm& term) {
  return is_name(ctx.machine, term, "\\+", 1) ? owned_goal(ctx, term.args[0]) : nullptr;
}

// M:G の G。M が翻訳のときに読み込み済みのモジュールのときだけ。無いモジュールや変数の M は、組み込みの :/2 が
// 呼び出しのときに確かめる（existence_error(module, M) など）。
const IrTerm* qualified_goal(ClauseCtx& ctx, const IrTerm& term) {
  if (!is_name(ctx.machine, term, ":", 2) || term.args[0].tag != IrTag::Atom) return nullptr;
  if (find_module(ctx.machine, term.args[0].atom) == nullptr) return nullptr;
  return owned_goal(ctx, term.args[1]);
}

// 構文の「条件」。-> と *-> の左、それを左に持つ選言の左の左、\+ の中、catch のゴール。他は nullptr。
const IrTerm* condition_of(ClauseCtx& ctx, const IrTerm& term) {
  const IrTerm* construct = &term;
  if (term.tag == IrTag::Or && term.args.size() == 2) construct = &term.args[0];
  if ((construct->tag == IrTag::IfThen || construct->tag == IrTag::SoftIf) && construct->args.size() == 2) {
    return &construct->args[0];
  }
  if (term.tag == IrTag::Catch && term.args.size() == 3) return &term.args[0];
  return negated_goal(ctx, term);
}

}  // namespace

const IrTerm* dead_end(ClauseCtx& ctx, const IrTerm& body, std::uint32_t module) {
  std::vector<GoalRef> goals;
  flatten(ctx, body, module, goals);
  for (const GoalRef& ref : goals) {
    if (ref.term->tag == IrTag::Fail || ref.term->tag == IrTag::Throw) return ref.term;
    if (ref.term->tag != IrTag::Cut) return nullptr;
  }
  return nullptr;
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
  // M:G は G の並びを M の文脈で置く（run_qualify、ADR 0006）。カットに透明なので境界はそのまま。
  if (const IrTerm* inner = qualified_goal(ctx, term); inner != nullptr) {
    return flatten(ctx, *inner, term.args[0].atom, out);
  }
  // 条件が呼び出し無しに終わる構文は、条件の中のカットが条件に閉じるので（7.8.7、7.8.8、8.15.1）、throw で終われば
  // その throw、fail で終われば else があればその枝、\+ なら true、他は fail と同じ。数える側と発出が同じ並びを
  // 見るように、ここで置き換える。
  const IrTerm* condition = condition_of(ctx, term);
  const IrTerm* end = condition != nullptr ? dead_end(ctx, *condition, module) : nullptr;
  // catch のゴールが throw で終わるのは例外を捕らえる普通の形。構文を残し、emit_catch が抜ける枠を作らない。
  if (end != nullptr && !(term.tag == IrTag::Catch && end->tag == IrTag::Throw)) {
    if (end->tag == IrTag::Throw) return out.push_back(GoalRef{end, module});
    if (term.tag == IrTag::Or) return flatten(ctx, term.args[1], module, out);
    if (term.tag == IrTag::Fun) return;
    ctx.owned.push_back(IrTerm{.tag = IrTag::Fail});
    out.push_back(GoalRef{&ctx.owned.back(), module});
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
      case IrTag::Catch:
        count_branch(ctx, goal.args[0], ref.module, counts);
        mark_slots(goal.args[1], counts);
        count_branch(ctx, goal.args[2], ref.module, counts);
        continue;
      default:
        if (const IrTerm* inner = negated_goal(ctx, goal); inner != nullptr) {
          count_branch(ctx, *inner, ref.module, counts);
        } else {
          mark_slots(goal, counts);
        }
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
      case IrTag::Throw: emit.line("return rt::throw_term(m, " + emit_build(emit, ref.term->args[0]) + ");"); return;
      case IrTag::Cut: emit.line("rt::cut_to(m, " + env.cut + ");"); continue;
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
  emit_head(emit, clause.head, predicate);
  emit_seq(ctx, emit, goals, BodyEnv{"r.cut", "rt::frame_cont(r, frame)"});
  const std::string entry = name.proc + "_c" + std::to_string(name.clause);
  code.functions += function_text(entry, emit.out);
  code.cases.emplace_back(entry_state, "return " + entry + "(m, r, frame);");
  return code;
}

}  // namespace mosa::codegen
