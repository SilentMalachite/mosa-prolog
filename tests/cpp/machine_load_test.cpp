// machine_test.cpp の続き（800 行の上限で分けた）。プログラムの読み込みを見る: discontiguous/1 と multifile/1、
// initialization/1、組込述語などの節を断ること、一般の指示。main は machine_test.cpp にあり、同じ実行ファイルに入る。
#include "harness.hpp"
#include "machine_support.hpp"
#include "mosa/machine.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

using machine_support::binding_text;

void check_discontiguous() {
  auto joined = mosa::Machine::create();
  mosa_test::check(joined->load_program(":- discontiguous(p/1). p(1). q(1). p(2).").ok, "gap allowed");
  auto first = joined->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more && binding_text(*joined, first) == "1",
                   "first clause");
  auto second = joined->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more && binding_text(*joined, second) == "2",
                   "second clause");
  mosa_test::check(joined->load_program("p(9).").ok, "replace after directive");
  auto only = joined->query("p(X).");
  mosa_test::check(only.kind == mosa::StopKind::Success && !only.more && binding_text(*joined, only) == "9",
                   "not kept");

  auto other = mosa::Machine::create();
  mosa_test::check(!other->load_program(":- discontiguous(p/1). q(1). r(1). q(2).").ok, "other stays apart");
  mosa_test::check(other->diagnostic().find("述語 q/1 の節が離れている") != std::string_view::npos, "other phrase");

  auto arity = mosa::Machine::create();
  mosa_test::check(!arity->load_program(":- discontiguous(p/2). p(1). q(1). p(2).").ok, "arity stays apart");
  mosa_test::check(arity->diagnostic().find("述語 p/1 の節が離れている") != std::string_view::npos, "arity phrase");

  auto late = mosa::Machine::create();
  mosa_test::check(!late->load_program("p(1). q(1). p(2). :- discontiguous(p/1).").ok, "directive after gap");
  mosa_test::check(late->diagnostic().find("述語 p/1 の節が離れている") != std::string_view::npos, "late phrase");

  auto bad = mosa::Machine::create();
  mosa_test::check(!bad->load_program(":- discontiguous(1).").ok, "bad indicator");
  mosa_test::check(bad->diagnostic().find("述語指示子ではありません") != std::string_view::npos, "indicator phrase");

  auto empty = mosa::Machine::create();
  mosa_test::check(empty->load_program(":- discontiguous([]).").ok, "empty list");
}

void check_multifile_add() {
  auto added = mosa::Machine::create();
  mosa_test::check(added->load_program(":- multifile(p/1). p(1).").ok, "multifile first");
  mosa_test::check(added->load_program("p(2).").ok, "multifile adds");
  auto first = added->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more && binding_text(*added, first) == "1",
                   "kept first");
  auto second = added->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more && binding_text(*added, second) == "2",
                   "added second");

  auto twice = mosa::Machine::create();
  mosa_test::check(twice->load_program(":- multifile(p/1). p(1).").ok, "twice base");
  mosa_test::check(twice->load_program(":- multifile(p/1). p(1).").ok, "twice again");
  auto one = twice->query("p(X).");
  mosa_test::check(one.kind == mosa::StopKind::Success && one.more && binding_text(*twice, one) == "1",
                   "twice first");
  auto again = twice->next();
  mosa_test::check(again.kind == mosa::StopKind::Success && !again.more && binding_text(*twice, again) == "1",
                   "twice second");

  auto rule = mosa::Machine::create();
  mosa_test::check(rule->load_program(":- multifile(p/1). p(1).").ok, "rule base");
  mosa_test::check(rule->load_program("p(X) :- X = 9.").ok, "rule adds");
  auto fact = rule->query("p(X).");
  mosa_test::check(fact.kind == mosa::StopKind::Success && fact.more && binding_text(*rule, fact) == "1",
                   "rule fact");
  auto body = rule->next();
  mosa_test::check(body.kind == mosa::StopKind::Success && !body.more && binding_text(*rule, body) == "9",
                   "rule body");
}

void check_multifile_scope() {
  auto neighbor = mosa::Machine::create();
  mosa_test::check(neighbor->load_program("q(1). :- multifile(p/1). p(1).").ok, "neighbor base");
  mosa_test::check(neighbor->load_program("q(2). p(2).").ok, "neighbor second");
  auto replaced = neighbor->query("q(X).");
  mosa_test::check(replaced.kind == mosa::StopKind::Success && !replaced.more &&
                       binding_text(*neighbor, replaced) == "2",
                   "neighbor replaced");
  auto kept = neighbor->query("p(X).");
  mosa_test::check(kept.kind == mosa::StopKind::Success && kept.more && binding_text(*neighbor, kept) == "1",
                   "neighbor kept");
  auto added = neighbor->next();
  mosa_test::check(added.kind == mosa::StopKind::Success && !added.more && binding_text(*neighbor, added) == "2",
                   "neighbor added");

  auto late = mosa::Machine::create();
  mosa_test::check(late->load_program("p(1).").ok, "late base");
  mosa_test::check(late->load_program("p(2). :- multifile(p/1).").ok, "late directive");
  auto only = late->query("p(X).");
  mosa_test::check(only.kind == mosa::StopKind::Success && !only.more && binding_text(*late, only) == "2",
                   "late replaces");
  mosa_test::check(late->load_program("p(3).").ok, "after late");
  auto follow = late->query("p(X).");
  mosa_test::check(follow.kind == mosa::StopKind::Success && follow.more && binding_text(*late, follow) == "2",
                   "late sticks");
  auto third = late->next();
  mosa_test::check(third.kind == mosa::StopKind::Success && !third.more && binding_text(*late, third) == "3",
                   "late next adds");

  auto arity = mosa::Machine::create();
  mosa_test::check(arity->load_program(":- multifile(p/2). p(1).").ok, "arity base");
  mosa_test::check(arity->load_program("p(2).").ok, "arity second");
  auto other = arity->query("p(X).");
  mosa_test::check(other.kind == mosa::StopKind::Success && !other.more && binding_text(*arity, other) == "2",
                   "arity replaces");
}

void check_multifile_guard() {
  auto gap = mosa::Machine::create();
  mosa_test::check(gap->load_program(":- multifile(p/1). p(1).").ok, "gap base");
  mosa_test::check(!gap->load_program("p(2). q(1). s(1). q(2).").ok, "gap still fails");
  mosa_test::check(gap->diagnostic().find("述語 q/1 の節が離れている") != std::string_view::npos, "gap phrase");
  auto still = gap->query("p(X).");
  mosa_test::check(still.kind == mosa::StopKind::Success && !still.more && binding_text(*gap, still) == "1",
                   "gap rolls clauses");

  auto rolled = mosa::Machine::create();
  mosa_test::check(!rolled->load_program(":- multifile(r/1). q(1). s(1). q(2).").ok, "rolled load");
  mosa_test::check(rolled->diagnostic().find("述語 q/1 の節が離れている") != std::string_view::npos,
                   "rolled phrase");
  mosa_test::check(rolled->load_program("r(1).").ok, "rolled first");
  mosa_test::check(rolled->load_program("r(2).").ok, "rolled second");
  auto single = rolled->query("r(X).");
  mosa_test::check(single.kind == mosa::StopKind::Success && !single.more && binding_text(*rolled, single) == "2",
                   "rolled replaces");

  auto bad = mosa::Machine::create();
  mosa_test::check(!bad->load_program(":- multifile(1).").ok, "bad indicator");
  mosa_test::check(bad->diagnostic().find("述語指示子ではありません") != std::string_view::npos,
                   "indicator phrase");

  // iso-notes 7.4.2 の決定 B。並びは受け付け、要素ごとに同じ検査をする。
  auto comma = mosa::Machine::create();
  mosa_test::check(comma->load_program(":- multifile((p/1, q/1)).").ok, "comma sequence");
  auto comma_bad = mosa::Machine::create();
  mosa_test::check(!comma_bad->load_program(":- multifile((p/1, 1)).").ok, "comma bad element");
  mosa_test::check(comma_bad->diagnostic().find("述語指示子ではありません") != std::string_view::npos,
                   "comma bad phrase");
  auto comma_list = mosa::Machine::create();
  mosa_test::check(!comma_list->load_program(":- dynamic((p/1, [q/1])).").ok, "comma holds no list");

  auto empty = mosa::Machine::create();
  mosa_test::check(empty->load_program(":- multifile([]).").ok, "empty multifile");

  auto alone = mosa::Machine::create();
  mosa_test::check(alone->load_program(":- multifile(p/1).").ok, "directive only");
  mosa_test::check(alone->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "no procedure");
  // iso-notes 7.8.3 の決定 B。宣言だけで手続きになり、節が無いので失敗する。
  mosa_test::check(alone->query("p(1).").kind == mosa::StopKind::Failure, "call declared only");
  auto apart = mosa::Machine::create();
  mosa_test::check(apart->load_program(":- discontiguous(p/1).").ok, "discontiguous only");
  mosa_test::check(apart->query("p(1).").kind == mosa::StopKind::Failure, "call discontiguous only");
  auto undone = mosa::Machine::create();
  mosa_test::check(!undone->load_program(":- discontiguous(p/1). :- multifile(q/1). :- dynamic(atom/1).").ok,
                   "failed load");
  mosa_test::check(undone->query("p(1).").kind == mosa::StopKind::Exception, "failed load drops discontiguous");
  mosa_test::check(undone->query("q(1).").kind == mosa::StopKind::Exception, "failed load drops multifile");
}

void check_multifile_forms() {
  auto listed = mosa::Machine::create();
  mosa_test::check(listed->load_program(":- multifile([a/1, b/0]). a(1). b.").ok, "list base");
  mosa_test::check(listed->load_program("a(2). b.").ok, "list adds");
  auto a = listed->query("a(X).");
  mosa_test::check(a.kind == mosa::StopKind::Success && a.more && binding_text(*listed, a) == "1", "list first");
  auto a2 = listed->next();
  mosa_test::check(a2.kind == mosa::StopKind::Success && !a2.more && binding_text(*listed, a2) == "2",
                   "list second");
  auto b = listed->query("b.");
  mosa_test::check(b.kind == mosa::StopKind::Success && b.more, "list atom first");
  auto b2 = listed->next();
  mosa_test::check(b2.kind == mosa::StopKind::Success && !b2.more, "list atom second");

  auto dynamic_kept = mosa::Machine::create();
  mosa_test::check(dynamic_kept->load_program(":- dynamic(p/1). p(1).").ok, "dynamic base");
  mosa_test::check(dynamic_kept->load_program(":- multifile(p/1). p(2).").ok, "dynamic adds");
  auto kept = dynamic_kept->query("p(X).");
  mosa_test::check(kept.kind == mosa::StopKind::Success && kept.more && binding_text(*dynamic_kept, kept) == "1",
                   "dynamic kept");
  auto added = dynamic_kept->next();
  mosa_test::check(added.kind == mosa::StopKind::Success && !added.more && binding_text(*dynamic_kept, added) == "2",
                   "dynamic second");
  mosa_test::check(dynamic_kept->query("assertz(p(3)).").kind == mosa::StopKind::Success, "stays dynamic");
  auto third = dynamic_kept->query("p(3).");
  mosa_test::check(third.kind == mosa::StopKind::Success && !third.more, "assert sees it");

  auto japanese = mosa::Machine::create();
  mosa_test::check(japanese->load_program(":- multifile(複/1). 複(1).").ok, "japanese first");
  mosa_test::check(japanese->load_program("複(2).").ok, "japanese adds");
  auto kana = japanese->query("複(X).");
  mosa_test::check(kana.kind == mosa::StopKind::Success && kana.more && binding_text(*japanese, kana) == "1",
                   "japanese kept");
  auto kana2 = japanese->next();
  mosa_test::check(kana2.kind == mosa::StopKind::Success && !kana2.more && binding_text(*japanese, kana2) == "2",
                   "japanese added");
}

void check_initialization_after() {
  auto machine = mosa::Machine::create();
  const bool ok = machine
                      ->load_program(":- dynamic(s/1).\n"
                                     ":- initialization(assertz(s(1))).\n"
                                     ":- initialization((p(X), assertz(s(X)))).\n"
                                     "p(2).\n"
                                     ":- initialization(assertz(s(3))).\n")
                      .ok;
  mosa_test::check(ok, "init after load");
  auto first = machine->query("s(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more && binding_text(*machine, first) == "1",
                   "init first");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && second.more && binding_text(*machine, second) == "2",
                   "init sees clause");
  auto third = machine->next();
  mosa_test::check(third.kind == mosa::StopKind::Success && !third.more && binding_text(*machine, third) == "3",
                   "init order");
  auto query_dir = mosa::Machine::create();
  mosa_test::check(query_dir->load_program(":- dynamic(s/1). ?- initialization(assertz(s(7))).").ok,
                   "query directive");
  auto seven = query_dir->query("s(X).");
  mosa_test::check(seven.kind == mosa::StopKind::Success && binding_text(*query_dir, seven) == "7",
                   "query directive runs");
}

void check_initialization_fail() {
  auto machine = mosa::Machine::create();
  const bool ok = machine
                      ->load_program("q(1).\n"
                                     ":- dynamic(s/1).\n"
                                     ":- initialization((assertz(s(1)), fail)).\n"
                                     ":- initialization(assertz(s(2))).\n")
                      .ok;
  mosa_test::check(ok, "fail still loads");
  mosa_test::check(machine->query("q(1).").kind == mosa::StopKind::Success, "fail keeps clause");
  auto first = machine->query("s(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more && binding_text(*machine, first) == "1",
                   "fail keeps effect");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more && binding_text(*machine, second) == "2",
                   "fail runs later");
  auto caught = mosa::Machine::create();
  const bool caught_ok = caught
                             ->load_program("kept(1).\n"
                                            ":- dynamic(s/1).\n"
                                            ":- initialization(catch(throw(1), _, true)).\n"
                                            ":- initialization(assertz(s(4))).\n")
                             .ok;
  mosa_test::check(caught_ok, "caught loads");
  mosa_test::check(caught->query("kept(1).").kind == mosa::StopKind::Success, "caught keeps clause");
  mosa_test::check(caught->query("s(4).").kind == mosa::StopKind::Success, "caught runs later");
}

void check_initialization_throw() {
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(
      "kept(1).\n"
      "also(1).\n"
      ":- dynamic(s/1).\n"
      ":- initialization((assertz(s(1)), throw(error(type_error(atom, 1), _)))).\n"
      ":- initialization(assertz(s(9))).\n");
  mosa_test::check(!status.ok, "throw fails load");
  mosa_test::check(machine->diagnostic() == "type_error(atom,1)", "throw formal");
  mosa_test::check(machine->query("kept(1).").kind == mosa::StopKind::Success, "throw keeps earlier");
  mosa_test::check(machine->query("also(1).").kind == mosa::StopKind::Success, "throw keeps later");
  mosa_test::check(machine->query("s(1).").kind == mosa::StopKind::Success, "throw keeps effect");
  mosa_test::check(machine->query("s(9).").kind == mosa::StopKind::Failure, "throw skips later");
  auto bare = mosa::Machine::create();
  const auto bare_status = bare->load_program("kept(1). :- initialization(throw(1)).");
  mosa_test::check(!bare_status.ok && bare->diagnostic() == "1", "throw ball");
  mosa_test::check(bare->query("kept(1).").kind == mosa::StopKind::Success, "ball keeps clause");
  auto missing = mosa::Machine::create();
  const auto missing_status = missing->load_program("kept(1). :- initialization(missing).");
  mosa_test::check(!missing_status.ok, "missing fails load");
  mosa_test::check(missing->diagnostic() == "existence_error(procedure,missing/0)", "missing formal");
  mosa_test::check(missing->query("kept(1).").kind == mosa::StopKind::Success, "missing keeps clause");
}

void check_initialization_args() {
  auto bad = mosa::Machine::create();
  const auto bad_status = bad->load_program("q(1). :- initialization(1).");
  mosa_test::check(!bad_status.ok, "number rejected");
  mosa_test::check(bad->diagnostic() == "type_error(callable,1)", "number formal");
  mosa_test::check(bad->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "number rolls back");
  auto open = mosa::Machine::create();
  const auto open_status = open->load_program(":- initialization(X).");
  mosa_test::check(!open_status.ok && open->diagnostic() == "instantiation_error", "var formal");
  // 本体に数を含むゴールも呼べない。call/1 と同じくゴール全体を名指し、動かさずに読み込みを戻す（iso-notes 7.4.2 の
  // initialization/1 の決定、レビュー M-16）。前はこの行が、前半を動かして葉の 1 を名指し節を残す形を固定していた。
  auto late = mosa::Machine::create();
  const auto late_status = late->load_program("kept(1). :- initialization((assertz(ran), 1)).");
  mosa_test::check(!late_status.ok, "body number fails");
  mosa_test::check(late->diagnostic() == "type_error(callable,(assertz(ran),1))", "body number formal");
  mosa_test::check(late->query("current_predicate(kept/1).").kind == mosa::StopKind::Failure, "body number rolls back");
  mosa_test::check(late->query("current_predicate(ran/0).").kind == mosa::StopKind::Failure, "body number does not run");
  auto two = mosa::Machine::create();
  const auto two_status = two->load_program(":- initialization(true, now).");
  mosa_test::check(!two_status.ok, "arity two rejected");
  mosa_test::check(two->diagnostic().find("この指示はフェーズ2では使えない") != std::string_view::npos,
                   "arity two phrase");
}

void check_initialization_live() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1). p(2).").ok, "live base");
  auto first = machine->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more, "live first");
  const auto depth = machine->choice_depth();
  mosa_test::check(machine->load_program(":- initialization((true; true)).").ok, "live init");
  mosa_test::check(machine->choice_depth() == depth, "live depth");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more && binding_text(*machine, second) == "2",
                   "live next");
  auto ja = mosa::Machine::create();
  mosa_test::check(ja->load_program(":- initialization(書く).\n書く :- assertz(印(1)).\n").ok, "ja init");
  mosa_test::check(ja->query("印(1).").kind == mosa::StopKind::Success, "ja effect");
  mosa_test::check(machine->query("initialization(true).").kind == mosa::StopKind::Exception, "not a goal");
}

// レビュー M10（iso-notes 7.4 の決定 A）。組込述語と制御構造の節、本体が呼べない節は、assertz/1 と同じ Formal で
// 読み込みを失敗させて節を戻す。両方に当たる節は、assertz/1 と同じく本体の誤りを先に出す（8.9.1.3）。
void check_refused_clauses() {
  constexpr std::array<std::pair<std::string_view, std::string_view>, 14> cases{{
      {"atom_length(_, 99).\n", "permission_error(modify,static_procedure,atom_length/2)"},
      {"call(_) :- write(mine).\n", "permission_error(modify,static_procedure,call/1)"},
      {"(a, b) :- true.\n", "permission_error(modify,static_procedure,(',')/2)"},
      {"! :- true.\n", "permission_error(modify,static_procedure,!/0)"},
      {"catch(_, _, _).\n", "permission_error(modify,static_procedure,catch/3)"},
      {"(a :- b) :- c.\n", "permission_error(modify,static_procedure,(:-)/2)"},
      {"(:- a) :- true.\n", "permission_error(modify,static_procedure,(:-)/1)"},
      {"user:atom_length(_, 99).\n", "permission_error(modify,static_procedure,atom_length/2)"},
      {"foo :- 1.\n", "type_error(callable,1)"},
      {"foo :- (true, 1).\n", "type_error(callable,(true,1))"},
      {"p :- (q ; 2.5).\n", "type_error(callable,(q;2.5))"},
      {"atom_length(_, _) :- 1.\n", "type_error(callable,1)"},
      // フェーズ10（ADR 0025）。int64 に入らない整数（Big）も数なので、本体に置けば呼べない（load_clauses.cpp の判定）。
      {"foo :- 123456789012345678901234567890.\n", "type_error(callable,123456789012345678901234567890)"},
      {"foo :- (true, 123456789012345678901234567890).\n", "type_error(callable,(true,123456789012345678901234567890))"},
  }};
  for (const auto& [text, formal] : cases) {
    auto machine = mosa::Machine::create();
    mosa_test::check(!machine->load_program("keep(1).\n" + std::string{text}).ok, "refused " + std::string{formal});
    mosa_test::check(machine->diagnostic() == formal, "refused formal " + std::string{formal});
    mosa_test::check(machine->query("keep(1).").kind == mosa::StopKind::Exception, "refused rolls back");
    mosa_test::check(
        machine->query("atom_length(abc, 3), \\+ current_predicate(atom_length/2).").kind == mosa::StopKind::Success,
        "refused builtin intact");
  }
  // SPEC 第5節。組込は system モジュールにあり、どのモジュールの中でも定義し直せない。
  auto module = mosa::Machine::create();
  mosa_test::check(!module->load_program(":- module(m10, []).\natom_length(_, 99).\n").ok, "refused in module");
  mosa_test::check(module->diagnostic() == "permission_error(modify,static_procedure,atom_length/2)",
                   "refused in module formal");
  for (const std::string_view text : {"foo(X) :- X.\n", "foo :- call(1).\n", "foo :- \\+ 1.\n", "[a].\n"}) {
    auto machine = mosa::Machine::create();
    mosa_test::check(machine->load_program(std::string{text}).ok, "accepted " + std::string{text});
  }
}

// iso-notes 7.4.2 の一般の指示。その位置で呼び、失敗と例外は読み込みを失敗させて節を戻す。
void check_general_directive() {
  auto ran = mosa::Machine::create();
  mosa_test::check(ran->load_program("q(1).\n:- q(X), assertz(seen(X)).\nq(2).\n").ok, "general runs");
  mosa_test::check(ran->query("findall(X, seen(X), [1]).").kind == mosa::StopKind::Success, "general sees earlier");
  auto failed = mosa::Machine::create();
  mosa_test::check(!failed->load_program("q(1).\n:- fail.\n").ok, "general fail rejects");
  mosa_test::check(failed->diagnostic().find("この指示のゴールは失敗した") != std::string_view::npos, "general fail phrase");
  mosa_test::check(failed->query("q(1).").kind == mosa::StopKind::Exception, "general fail rolls back");
  auto threw = mosa::Machine::create();
  mosa_test::check(!threw->load_program("q(1).\n:- throw(oops).\n").ok, "general throw rejects");
  mosa_test::check(threw->diagnostic() == "oops", "general throw formal");
  mosa_test::check(threw->query("q(1).").kind == mosa::StopKind::Exception, "general throw rolls back");
  auto module = mosa::Machine::create();
  mosa_test::check(module->load_program(":- module(gm, []).\nr(1).\n:- r(1).\n").ok, "general module context");
}
