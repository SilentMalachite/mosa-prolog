// 機械の問い合わせ、制御、項と型、索引、ゴールの語を見る。プログラムの読み込み（multifile/1、initialization/1、
// 断る節、一般の指示）は machine_load_test.cpp にあり、同じ実行ファイルに入る（800 行の上限で分けた）。
#include "harness.hpp"
#include "machine_support.hpp"
#include "mosa/goal_word.hpp"
#include "mosa/machine.hpp"

#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace {

using machine_support::binding_text;

void check_one_fact() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(a).").ok, "load fact");
  auto yes = machine->query("p(a).");
  mosa_test::check(yes.kind == mosa::StopKind::Success && !yes.more, "fact hits");
  auto no = machine->query("p(b).");
  mosa_test::check(no.kind == mosa::StopKind::Failure, "fact misses");
  auto bound = machine->query("p(X).");
  mosa_test::check(bound.kind == mosa::StopKind::Success && bound.bindings.size() == 1, "fact binds");
  mosa_test::check(bound.bindings[0].name == "X", "name X");
  mosa_test::check(binding_text(*machine, bound) == "a", "value a");
}

// 選択点を残した問い合わせを次の問い合わせが閉じるとき、setup_call_cleanup/3 の後始末を動かす（ADR 0016、レビュー M11）。
// 閉じるときは Goal の束縛が見える。next() で解を尽くせば、そのとき失敗の後始末として 1 回だけ動く。
void check_cleanup_on_close() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("g(1). g(2).").ok, "cleanup load");
  auto open = machine->query("setup_call_cleanup(true, g(X), nb_setval(m11_closed, X)).");
  mosa_test::check(open.kind == mosa::StopKind::Success && open.more, "cleanup goal left a choice point");
  auto none = machine->query("catch(nb_getval(m11_closed, _), _, fail).");
  mosa_test::check(none.kind == mosa::StopKind::Success, "closing the query ran the cleanup");
  auto seen = machine->query("nb_getval(m11_closed, V).");
  mosa_test::check(seen.kind == mosa::StopKind::Success && binding_text(*machine, seen) == "1", "cleanup saw X = 1");
  auto again = machine->query("setup_call_cleanup(true, g(_), nb_setval(m11_count, done)).");
  mosa_test::check(again.kind == mosa::StopKind::Success && again.more, "second goal open");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more, "second answer is the last");
  auto last = machine->next();
  mosa_test::check(last.kind == mosa::StopKind::Failure, "no third answer");
  auto count = machine->query("nb_getval(m11_count, V).");
  mosa_test::check(count.kind == mosa::StopKind::Success && binding_text(*machine, count) == "done", "cleanup ran on failure");
  // 敵対的レビュー A5。halt で止まった問い合わせの後始末は、次の問い合わせが閉じるときも動かさない（ADR 0016）。
  auto reset = machine->query("nb_setval(m11_halted, none).");
  mosa_test::check(reset.kind == mosa::StopKind::Success, "reset before halt");
  auto halted = machine->query("setup_call_cleanup(true, g(_), nb_setval(m11_halted, ran)), halt.");
  mosa_test::check(halted.kind == mosa::StopKind::Halt, "goal halted with a cleanup armed");
  auto after = machine->query("nb_getval(m11_halted, V).");
  mosa_test::check(after.kind == mosa::StopKind::Success && binding_text(*machine, after) == "none", "halt runs no cleanup");
  // Codex レビュー。-> の確定が外した後始末は、then の halt より先に動く。
  auto committed = machine->query("setup_call_cleanup(true, (true ; true), nb_setval(m11_commit, ran)) -> halt.");
  mosa_test::check(committed.kind == mosa::StopKind::Halt, "then branch halted");
  auto ran = machine->query("nb_getval(m11_commit, V).");
  mosa_test::check(ran.kind == mosa::StopKind::Success && binding_text(*machine, ran) == "ran", "commit ran the cleanup before halt");
}

// 旗は前のクエリで真にし、次のクエリの読みから効く（SPEC 第4節）。
void check_fullwidth_query() {
  auto machine = mosa::Machine::create();
  auto set = machine->query("set_prolog_flag(japanese_punctuation, true).");
  mosa_test::check(set.kind == mosa::StopKind::Success, "set fullwidth flag");
  auto read = machine->query("X = f（a）。");
  mosa_test::check(read.kind == mosa::StopKind::Success && binding_text(*machine, read) == "f(a)",
                   "query reads fullwidth punctuation");
}

void check_choice_and_nrev() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1). p(2).").ok, "two facts");
  auto first = machine->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more, "first choice");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more, "second choice");
  auto branch = machine->query("(true ; fail).");
  mosa_test::check(branch.kind == mosa::StopKind::Success && branch.more, "disjunction");
  auto branch_next = machine->next();
  mosa_test::check(branch_next.kind == mosa::StopKind::Failure, "disjunction ends");
  mosa_test::check(machine->load_program(
                       "nrev([], []). nrev([H|T], R) :- nrev(T, S), append(S, [H], R). "
                       "append([], L, L). append([H|T], L, [H|R]) :- append(T, L, R).")
                       .ok,
                   "load nrev");
  auto rev = machine->query("nrev([1, 2, 3], R).");
  mosa_test::check(rev.kind == mosa::StopKind::Success && rev.bindings.size() == 1 &&
                       rev.bindings[0].name == "R",
                   "nrev binds");
  mosa_test::check(binding_text(*machine, rev) == "[3,2,1]", "nrev text");
}

void check_cut() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p :- !, fail. p.").ok, "load cut");
  mosa_test::check(machine->query("p.").kind == mosa::StopKind::Failure, "cut drops second clause");
  mosa_test::check(machine->load_program("q(X) :- (X = 1 ; X = 2), !.").ok, "load local cut");
  auto once = machine->query("q(X).");
  mosa_test::check(once.kind == mosa::StopKind::Success && !once.more && binding_text(*machine, once) == "1",
                   "cut drops sibling");
  mosa_test::check(machine->load_program("r(X) :- true -> X = 1 ; X = 2. s(X) :- fail -> X = 1 ; X = 2.").ok,
                   "load if then");
  auto then = machine->query("r(X).");
  mosa_test::check(then.kind == mosa::StopKind::Success && !then.more && binding_text(*machine, then) == "1",
                   "then commits");
  auto otherwise = machine->query("s(X).");
  mosa_test::check(otherwise.kind == mosa::StopKind::Success && !otherwise.more &&
                       binding_text(*machine, otherwise) == "2",
                   "else runs");
}

void check_catch() {
  auto machine = mosa::Machine::create();
  auto caught = machine->query("catch(throw(a), a, true).");
  mosa_test::check(caught.kind == mosa::StopKind::Success, "catch same ball");
  auto missed = machine->query("catch(throw(b), a, true).");
  mosa_test::check(missed.kind == mosa::StopKind::Exception, "catch misses");
  try {
    auto tiny = mosa::Machine::create(8);
    auto boom = tiny->query("catch(p(a), _, true).");
    mosa_test::check(boom.kind != mosa::StopKind::Success && boom.kind != mosa::StopKind::Exception,
                     "resource is not a prolog ball");
  } catch (const mosa::ResourceLimit&) {
    mosa_test::check(true, "resource is not a prolog ball");
  }
}

// レビュー M13。左の子から自分に戻る巡回するゴールは、前は無限の左再帰として動き、ヒープの上限で止まった（catch で
// 受けられない）。2026-11-17 の持ち主の決定 A（iso-notes 7.3.3）から、訳す時に resource_error(cyclic_term) を投げ、
// 小さなヒープでも上限に届かずに catch で受けられる。(X ; true) も同じ（前は上限で止まらなかった、レビュー M17）。
void check_cyclic_goal_error() {
  for (const char* goal : {"X = (X, true), call(X)", "X = catch(X, _, true), call(X)",
                           "X = (X -> true ; true), call(X)", "X = (\\+ X), call(X)", "X = (X, true), X",
                           "X = (X ; true), call(X)"}) {
    auto tiny = mosa::Machine::create(std::size_t{1} << 16);
    const std::string query = "catch((" + std::string{goal} + "), error(resource_error(cyclic_term), _), true).";
    bool caught = false;
    try {
      caught = tiny->query(query).kind == mosa::StopKind::Success;
    } catch (const mosa::ResourceLimit&) {
      caught = false;
    }
    mosa_test::check(caught, std::string{goal} + ": raises resource_error(cyclic_term)");
  }
}

// length/2 が作るリストはヒープの上限で止まる。先に vector を確保すると length_error で落ちる。
void check_length_limit() {
  auto tiny = mosa::Machine::create(4096);
  bool limited = false;
  try {
    (void)tiny->query("length(_, 9223372036854775807).");
  } catch (const mosa::ResourceLimit&) {
    limited = true;
  } catch (const std::exception&) {
    limited = false;
  }
  mosa_test::check(limited, "length/2 with a huge N hits the heap limit");
}

void check_types() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("var(X).").kind == mosa::StopKind::Success, "var");
  mosa_test::check(machine->query("var(a).").kind == mosa::StopKind::Failure, "not var");
  mosa_test::check(machine->query("atom([]).").kind == mosa::StopKind::Success, "nil atom");
  mosa_test::check(machine->query("integer(1.0).").kind == mosa::StopKind::Failure, "float is not integer");
  mosa_test::check(machine->query("compound([]).").kind == mosa::StopKind::Failure, "nil not compound");
  mosa_test::check(machine->query("callable(1).").kind == mosa::StopKind::Failure, "callable number");
  mosa_test::check(machine->query("callable(X).").kind == mosa::StopKind::Failure, "callable var");
  mosa_test::check(machine->query("is_list([a]).").kind == mosa::StopKind::Success, "proper list");
  mosa_test::check(machine->query("is_list([a|b]).").kind == mosa::StopKind::Failure, "partial list");
  mosa_test::check(machine->query("is_list(X).").kind == mosa::StopKind::Failure, "unbound list");
  // is_list は背骨だけを見る。背骨が循環すれば偽、要素が循環しても背骨が [] で終われば真（レビュー L3 の決定 A）。
  auto cycle = machine->query("(L = [a|L], is_list(L)).");
  mosa_test::check(cycle.kind == mosa::StopKind::Failure, "cyclic list");
  mosa_test::check(machine->query("(X = [X], is_list(X)).").kind == mosa::StopKind::Success,
                   "list with a cyclic element");
  mosa_test::check(machine->query("(X = f(X), unify_with_occurs_check(Y, f(Y))).").kind ==
                       mosa::StopKind::Failure,
                   "occurs check fails");
  mosa_test::check(machine->query("missing(1).").kind == mosa::StopKind::Exception, "unknown predicate");
  mosa_test::check(machine->query("X = f(X).").kind == mosa::StopKind::Success, "no occurs on equals");
  mosa_test::check(machine->query("X = [X].").kind == mosa::StopKind::Success, "no occurs on list");
}

void check_order() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("(1.0 @< 1).").kind == mosa::StopKind::Success, "float before integer");
  mosa_test::check(machine->query("(1 == 1.0).").kind == mosa::StopKind::Failure, "term equality is not numeric");
  mosa_test::check(machine->query("(0.0 @< -0.0).").kind == mosa::StopKind::Failure, "0.0 is not before -0.0");
  mosa_test::check(machine->query("compare(=, a, a).").kind == mosa::StopKind::Success, "compare same");
  mosa_test::check(machine->query("(a @< b).").kind == mosa::StopKind::Success, "atom order");
  mosa_test::check(machine->query("(X = f(X), Y = f(Y), X @< Y).").kind == mosa::StopKind::Failure,
                   "cyclic order stops");
}

void check_terms() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("functor(f(a, b), f, 2).").kind == mosa::StopKind::Success, "functor");
  mosa_test::check(machine->query("arg(1, f(a, b), a).").kind == mosa::StopKind::Success, "arg");
  mosa_test::check(machine->query("(f(a, b) =.. [f, a, b]).").kind == mosa::StopKind::Success, "univ");
  mosa_test::check(machine->query("(copy_term(f(A), f(B)), A = 1, var(B)).").kind == mosa::StopKind::Success,
                   "copy shares no var");
  mosa_test::check(machine->query("term_variables(f(A, A, B), [A, B]).").kind == mosa::StopKind::Success,
                   "term variables");
  mosa_test::check(machine->query("(T =.. ['.', a, []], T = [a], T == [a], is_list(T)).").kind ==
                       mosa::StopKind::Success,
                   "dot functor is a list");
  mosa_test::check(machine->query("(functor(T, '.', 2), arg(1, T, a), arg(2, T, []), T = [a]).").kind ==
                       mosa::StopKind::Success,
                   "functor dot builds a list");
}

void check_answer_and_consult() {
  auto machine = mosa::Machine::create();
  auto cycle = machine->query("X = [X].");
  mosa_test::check(mosa::format_answer(*machine, cycle) == "X = [X]", "cyclic answer");
  auto tail = machine->query("X = [a|X].");
  mosa_test::check(mosa::format_answer(*machine, tail) == "X = [a|X]", "cyclic tail name");
  auto named_tail = machine->query("(X = [a|Y], Y = b).");
  mosa_test::check(mosa::format_answer(*machine, named_tail) == "X = [a|Y], Y = b", "named list tail");
  auto alias = machine->query("X = Y.");
  mosa_test::check(mosa::format_answer(*machine, alias) == "X = Y", "alias once");
  auto both = machine->query("(X = Y, Y = 1).");
  mosa_test::check(mosa::format_answer(*machine, both) == "X = 1, Y = 1", "both ground");
  mosa_test::check(machine->load_program("p(1).").ok, "first p");
  mosa_test::check(machine->load_program("p(2).").ok, "replace p");
  auto replaced = machine->query("p(X).");
  mosa_test::check(replaced.kind == mosa::StopKind::Success && !replaced.more &&
                       binding_text(*machine, replaced) == "2",
                   "not doubled");
  auto broken = mosa::Machine::create();
  mosa_test::check(!broken->load_program("q(1). r(1). q(2).").ok, "discontiguous q");
  mosa_test::check(broken->diagnostic().find("述語 q/1 の節が離れている") != std::string_view::npos,
                   "discontiguous phrase");
}

void check_arith() {
  auto quot_machine = mosa::Machine::create();
  auto quot = quot_machine->query("X is -5 // 2.");
  const auto q = binding_text(*quot_machine, quot);
  mosa_test::check(quot.kind == mosa::StopKind::Success && q == "-2", "toward zero is -2");
  auto floor_machine = mosa::Machine::create();
  auto floor = floor_machine->query("X is div(-5, 2).");
  const auto d = binding_text(*floor_machine, floor);
  mosa_test::check(floor.kind == mosa::StopKind::Success && d == "-3", "floor div is -3");
  auto errors = mosa::Machine::create();
  mosa_test::check(errors->query("X is 1 / 0.").kind == mosa::StopKind::Exception, "zero divisor");
  mosa_test::check(errors->query("X is 9223372036854775807 + 1.").kind == mosa::StopKind::Success,
                   "past max_integer is a Big");
  mosa_test::check(errors->query("(1.0 =:= 1).").kind == mosa::StopKind::Success, "numeric equal");
}

void check_control() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("(call(!), fail ; true).").kind == mosa::StopKind::Success,
                   "cut stays inside call");
  mosa_test::check(machine->query("(\\+ fail).").kind == mosa::StopKind::Success, "not fail");
  mosa_test::check(machine->query("(\\+ true).").kind == mosa::StopKind::Failure, "not true");
  auto halted = machine->query("halt(3).");
  mosa_test::check(halted.kind == mosa::StopKind::Halt && halted.halt_code == 3, "halt code");
  // ADR 0014。Call の節点として call/1 で動く catch の Goal と Recovery の中の halt/1 も、機械を止めて終了コードを返す。
  constexpr std::string_view late = "lh(X) :- catch((X, true), _, true). lr(X) :- catch(throw(e), _, (X, true)).";
  for (const auto& [goal, code] : {std::pair{"lh(halt(4)).", 4}, std::pair{"lr(halt(5)).", 5}}) {
    auto stopped = mosa::Machine::create();
    mosa_test::check(stopped->load_program(late).ok, std::string{goal} + ": load");
    const auto stop = stopped->query(goal);
    mosa_test::check(stop.kind == mosa::StopKind::Halt && stop.halt_code == code, std::string{goal} + ": halt code");
  }
  // レビュー M15。呼べない部分のある catch の Goal と Recovery は、halt/1 を動かす前に型エラーで捕まえられる。
  constexpr std::string_view early = "nh :- catch((halt(6), 1), error(type_error(callable, _), _), true)."
                                     " nr :- catch(throw(e), _, (halt(7), 1)).";
  auto checked = mosa::Machine::create();
  mosa_test::check(checked->load_program(early).ok, "nh nr: load");
  mosa_test::check(checked->query("nh.").kind == mosa::StopKind::Success, "nh: no halt");
  mosa_test::check(checked->query("nr.").kind == mosa::StopKind::Exception, "nr: no halt");
  auto again = mosa::Machine::create();
  mosa_test::check(again->query("call(1).").kind == mosa::StopKind::Exception, "call number");
  mosa_test::check(again->load_program(":- op(600, xfy, *->). p(1). p(2).").ok, "soft cut data");
  auto soft = again->query("(p(X) *-> true ; X = 9).");
  mosa_test::check(soft.kind == mosa::StopKind::Success && soft.more, "soft cut keeps solutions");
}

void check_index() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(a). p(b).").ok, "index load");
  auto one = machine->query("p(a).");
  mosa_test::check(one.kind == mosa::StopKind::Success && !one.more, "single candidate");
  auto any = machine->query("p(X).");
  mosa_test::check(any.kind == mosa::StopKind::Success && any.more, "variable tries all");
  mosa_test::check(machine->load_program("q(a). q(X) :- X = c.").ok, "var head kept");
  auto q = machine->query("q(a).");
  mosa_test::check(q.kind == mosa::StopKind::Success && q.more, "variable head remains");
}

bool atom_named(mosa::Machine& machine, mosa::Cell cell, std::string_view name) {
  cell = machine.heap().deref(cell);
  if (cell.tag() != mosa::Tag::Atom) return false;
  return machine.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
}

bool functor_named(mosa::Machine& machine, mosa::Cell cell, std::string_view name, int arity) {
  cell = machine.heap().deref(cell);
  return cell.tag() == mosa::Tag::Str && machine.heap().functor_arity(cell) == static_cast<unsigned>(arity) &&
         machine.atoms().name(machine.heap().functor_atom(cell)) == name;
}

bool dot_procedure(mosa::Machine& machine, mosa::Cell pi) {
  pi = machine.heap().deref(pi);
  if (!functor_named(machine, pi, "/", 2)) return false;
  const auto arity = machine.heap().deref(machine.heap().arg(pi, 1));
  return atom_named(machine, machine.heap().arg(pi, 0), ".") &&
         (arity.tag() == mosa::Tag::Int || arity.tag() == mosa::Tag::IntBox) &&
         machine.heap().integer_value(arity) == 2;
}

bool dot_missing(mosa::Machine& machine, const mosa::Stop& stop) {
  if (stop.kind != mosa::StopKind::Exception) return false;
  const mosa::Cell error = machine.heap().deref(stop.exception);
  if (!functor_named(machine, error, "error", 2)) return false;
  const mosa::Cell formal = machine.heap().deref(machine.heap().arg(error, 0));
  if (!functor_named(machine, formal, "existence_error", 2)) return false;
  if (!atom_named(machine, machine.heap().arg(formal, 0), "procedure")) return false;
  if (!dot_procedure(machine, machine.heap().arg(formal, 1))) return false;
  const mosa::Cell context = machine.heap().deref(machine.heap().arg(error, 1));
  return functor_named(machine, context, "context", 2) && dot_procedure(machine, machine.heap().arg(context, 0));
}

void check_list_goal() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("callable([a]).").kind == mosa::StopKind::Success, "list is callable");
  mosa_test::check(dot_missing(*machine, machine->query("'.'(a, []).")), "dot goal is '.'/2");
  mosa_test::check(dot_missing(*machine, machine->query("[a].")), "list goal is '.'/2");
  mosa_test::check(dot_missing(*machine, machine->query("call([a]).")), "call list is '.'/2");
  mosa_test::check(dot_missing(*machine, machine->query("(X = [a], call(X)).")), "bound list is '.'/2");
  auto listed = mosa::Machine::create();
  mosa_test::check(listed->load_program("[a].").ok, "list clause");
  mosa_test::check(listed->query("[a].").kind == mosa::StopKind::Success, "defined list runs");
  mosa_test::check(listed->query("'.'(a, []).").kind == mosa::StopKind::Success, "list clause via dot");
  auto dotted = mosa::Machine::create();
  mosa_test::check(dotted->load_program("'.'(b, []).").ok, "dot clause");
  mosa_test::check(dotted->query("[b].").kind == mosa::StopKind::Success, "defined dot runs");
  mosa_test::check(dotted->query("'.'(b, []).").kind == mosa::StopKind::Success, "defined dot syntax");
}

void check_goal_word() {
  const auto word = mosa::pack_goal_word(3, 17);
  mosa_test::check(mosa::goal_word_arity(word) == 3, "frame_module_roundtrip arity");
  mosa_test::check(mosa::goal_word_module(word) == 17, "frame_module_roundtrip module");
  const auto edge = mosa::pack_goal_word(65535, 0xFFFFFFFFU);
  mosa_test::check(mosa::goal_word_arity(edge) == 65535, "frame_arity_max arity");
  mosa_test::check(mosa::goal_word_module(edge) == 0xFFFFFFFFU, "frame_arity_max module");
  mosa_test::check((edge >> 48) == 0, "frame_arity_max high bits stay clear");
}

}  // namespace

// ADR 0008。走査中の abolish で残った枠を、後の読み込みが新しい静的な定義として使い直す。
void check_abolished_reload() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(":- dynamic(p/1). p(1). p(2).").ok, "abolish base");
  mosa_test::check(machine->query("(p(X), abolish(p/1), fail ; true).").kind == mosa::StopKind::Success,
                   "abolish in scan");
  mosa_test::check(machine->query("p(_).").kind == mosa::StopKind::Exception, "abolished is absent");
  mosa_test::check(machine->load_program("p(5).").ok, "reload abolished");
  mosa_test::check(machine->query("findall(X, p(X), [5]).").kind == mosa::StopKind::Success, "reload clauses");
  mosa_test::check(machine->query("predicate_property(p(_), static).").kind == mosa::StopKind::Success,
                   "reload static");
  mosa_test::check(machine->query("predicate_property(p(_), number_of_clauses(1)).").kind ==
                       mosa::StopKind::Success,
                   "reload count");
}

void check_discontiguous();
void check_multifile_add();
void check_multifile_scope();
void check_multifile_guard();
void check_multifile_forms();
void check_initialization_after();
void check_initialization_fail();
void check_initialization_throw();
void check_initialization_args();
void check_initialization_live();
void check_refused_clauses();
void check_general_directive();

int main() {
  check_goal_word();
  check_one_fact();
  check_fullwidth_query();
  check_choice_and_nrev();
  check_cut();
  check_catch();
  check_types();
  check_order();
  check_length_limit();
  check_cyclic_goal_error();
  check_terms();
  check_answer_and_consult();
  check_discontiguous();
  check_multifile_add();
  check_multifile_scope();
  check_multifile_guard();
  check_multifile_forms();
  check_initialization_after();
  check_initialization_fail();
  check_initialization_throw();
  check_initialization_args();
  check_initialization_live();
  check_general_directive();
  check_refused_clauses();
  check_arith();
  check_control();
  check_index();
  check_list_goal();
  check_abolished_reload();
  check_cleanup_on_close();
  return mosa_test::failures();
}
