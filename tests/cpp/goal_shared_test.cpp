// レビュー M28 の Codex 敵対的レビュー（3 回目）。実行時のゴールのスロットが 2^16 個を超えそうで、後で訳せない葉
// （ここでは未束縛の変数のゴール 3000 個）が残りに並ぶと、compile_goal は最後の段ですべてを写す。共有の多い引数を
// 木に広げる写しは、ヒープの残りを超えたところで上限で止まる（前は止まらずに何 GB も使った）。上限は 8 MiB。
// 後で訳せる形なら、同じ大きさのゴールが 8 MiB に収まって成功する。
// Codex 敵対的レビュー（5 回目）。共有する制御構造はどの段でも木に広げるので、深さ 20 の (T, T) は 2^20 個の
// 節点になる。前は最初の fail で失敗して、上限に数える前に 121 MB を使った。今は組む間に上限で止まる。
// Codex 敵対的レビュー（6 回目）。fail の葉も節点に数える（深さ 17 では枠 131071 個だけなら予算に収まり、葉を
// 足すと超える。IrTerm 1 つは 7 語なので、8 MiB の予算は 149796 個）。前に訳したゴールが状態の語に残っていても、
// 次の同じ大きさのゴール（予算の 3 分の 2）は止めない。前は状態の語を予算から引いて、GC の前に止めた。
// Codex 敵対的レビュー（7 回目）。すべて写す段で、循環の入口は Var 1 つに縮む。別のセルの引数 f(C) 69930 個は
// IR に 2 節点ずつ残るが、前は縮める前に歩いた循環の節点まで数えて上限で止めた。C は長さ 64 の循環 1 つか、
// 別の循環 10000 個を順に回したもの。上限は 32 MiB（予算は 599186 節点）。数えるのは、縮める節点を引けば約 15 万、
// 引かなければ 1 つで 460 万（入口を指し直さないとき）、10000 個で約 78 万。入口は 1 つずつスロットを取るので、2^16
// 個を超えない数にする（Codex 敵対的レビュー 8 回目。超えるとスロットの番号が折り返す。M31）。頭の cw/1000 は、
// どの引数も f をたどって 64 歩までに元のセルに戻ることを確かめる。
// Codex 敵対的レビュー（9 回目と 10 回目）。C = f(C, X) と C = f(X, C) の C は入口なので Var 1 つに縮む。前は縮める
// 前に、捨てる X（深さ 40 の共有項）まで木に広げて上限で止めた。今は写す前に入口を印す（Build::mark_loops）。
// Codex 敵対的レビュー（11 回目）。C = f(D), D = f(C, X) で C を先に訪ねると、戻り辺の先 C だけが印され、D の写しが
// X を広げた。今は循環する強連結成分の節点をすべて印す。
// 手元の GC ストレス（reconsult が落ちた）。失敗した問い合わせは ip に空いた枠の番号を残し、次の load_program の指示が
// それを外の枠として退避して GC の根にした。GC は空いた番地の語を枠と読み、指示のゴール枠の引数を捨てて落ちた。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace {

// 語で数える。8 MiB。
constexpr std::size_t k_limit = std::size_t{1} << 20;

// 引数 1000 個のゴール 70 個の連言。引数は別のセルの f(E) で、E は深さ 40 の共有項（tests/pl/control.pl の m28_*）。
constexpr std::string_view k_program =
    "dag(0, a) :- !.\n"
    "dag(N, f(T, T)) :- M is N - 1, dag(M, T).\n"
    "fs(0, _, [], []) :- !.\n"
    "fs(N, E, [f(_)|T], [f(E)|U]) :- M is N - 1, fs(M, E, T, U).\n"
    "wide(_, _, _, 0, true) :- !.\n"
    "wide(Fs, As, V, N, (G, Rest)) :-\n"
    "    copy_term(Fs, Args), Args = As, G =.. [w, V|Args], M is N - 1, wide(Fs, As, V, M, Rest).\n"
    "wide_goal(E, V, G) :-\n"
    "    fs(999, E, Fs, As), copy_term(Fs, HeadArgs), H =.. [w, W|HeadArgs], assertz((H :- W == v)),\n"
    "    wide(Fs, As, V, 70, G).\n"
    "vars(0, [], true) :- !.\n"
    "vars(N, [C|Cs], (C, R)) :- M is N - 1, vars(M, Cs, R).\n"
    "bind([]).\n"
    "bind([true|T]) :- bind(T).\n"
    "over :- dag(40, X), wide_goal(X, V, G), vars(3000, Cs, Vars), call((bind(Cs), V = v, G, Vars)).\n"
    "fits :- dag(40, X), wide_goal(X, V, G), call((V = v, G)).\n"
    "skel(0, G, G) :- !.\n"
    "skel(N, G, (T, T)) :- M is N - 1, skel(M, G, T).\n"
    "shared_skeleton :- skel(20, fail, G), \\+ call(G).\n"
    "small_skeleton :- skel(8, fail, G), \\+ call(G).\n"
    "leaf_skeleton :- skel(17, fail, G), \\+ call(G).\n"
    "leaf(a).\n"
    "twice :- skel(15, leaf(a), G), call(G), call(G).\n"
    "cyc_all([]).\n"
    "cyc_all([f(C)|T]) :- C = f(D), back(D, C, 64), cyc_all(T).\n"
    "back(D, C, _) :- D == C, !.\n"
    "back(f(D), C, N) :- N > 0, M is N - 1, back(D, C, M).\n"
    "cw_head :-\n"
    "    fs(999, a, Fs, _), copy_term(Fs, Args), H =.. [cw, W|Args], retractall(H),\n"
    "    assertz((H :- W == v, cyc_all(Args))).\n"
    "ring(0, X, X) :- !.\n"
    "ring(N, X, f(T)) :- M is N - 1, ring(M, X, T).\n"
    "cycles(0, []) :- !.\n"
    "cycles(K, [X|T]) :- ring(64, X, R), X = R, M is K - 1, cycles(M, T).\n"
    "pick(0, Cs, _, [], Cs) :- !.\n"
    "pick(N, [], All, As, Cs) :- !, pick(N, All, All, As, Cs).\n"
    "pick(N, [C|Cs0], All, [f(C)|As], Cs) :- M is N - 1, pick(M, Cs0, All, As, Cs).\n"
    "cwide(_, _, _, 0, true) :- !.\n"
    "cwide(Cs0, All, V, N, (G, Rest)) :-\n"
    "    pick(999, Cs0, All, Args, Cs), G =.. [cw, V|Args], M is N - 1, cwide(Cs, All, V, M, Rest).\n"
    "cyclic_run(All) :- cw_head, cwide(All, All, V, 70, G), vars(3000, Bs, Vars), call((bind(Bs), V = v, G, Vars)).\n"
    "cyclic :- cycles(1, All), cyclic_run(All).\n"
    "cyclic_distinct :- cycles(10000, All), cyclic_run(All).\n"
    "cyclic_dag :- dag(40, X), C = f(C, X), wide_goal(C, V, G), vars(3000, Bs, Vars), call((bind(Bs), V = v, G, Vars)).\n"
    "cyclic_dag_late :-\n"
    "    dag(40, X), C = f(X, C), wide_goal(C, V, G), vars(3000, Bs, Vars), call((bind(Bs), V = v, G, Vars)).\n"
    "cyclic_scc :-\n"
    "    dag(40, X), C = f(D), D = f(C, X), wide_goal(D, V, G), vars(3000, Bs, Vars),\n"
    "    call((C \\== b, bind(Bs), V = v, G, Vars)).\n";

struct Outcome {
  bool limited = false;
  mosa::StopKind kind = mosa::StopKind::Failure;
};

Outcome run(mosa::Machine& machine, std::string_view goal) {
  Outcome outcome;
  try {
    outcome.kind = machine.query(goal).kind;
  } catch (const mosa::ResourceLimit&) {
    outcome.limited = true;
  }
  return outcome;
}

// 失敗した問い合わせの後に、GC を呼ぶ指示を読む。どちらの問い合わせも、直す前は次の指示で落ちた。
void check_failed_query_then_directive() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(":- dynamic(p/1).\n").ok, "failed query: load");
  for (const std::string_view goal : {"X = f(Y), fail.", "p(_)."}) {
    mosa_test::check(machine->query(goal).kind == mosa::StopKind::Failure, "failed query: the query fails");
    mosa_test::check(machine->load_program(":- garbage_collect, retractall(q(_)), assertz(q(f(2))).\n").ok,
                     "failed query: a directive that runs GC loads");
    mosa_test::check(machine->query("q(f(2)).").kind == mosa::StopKind::Success,
                     "failed query: the directive asserted its argument");
  }
}

}  // namespace

int main() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "load");
  const Outcome over = run(*machine, "over.");
  mosa_test::check(over.limited, "a copy that spreads the sharing stops at the heap limit");
  const Outcome fits = run(*machine, "fits.");
  mosa_test::check(!fits.limited && fits.kind == mosa::StopKind::Success, "the deferred goal fits in 8 MiB");
  const Outcome shared = run(*machine, "shared_skeleton.");
  mosa_test::check(shared.limited, "a shared control skeleton stops at the heap limit while it is compiled");
  const Outcome small = run(*machine, "small_skeleton.");
  mosa_test::check(!small.limited && small.kind == mosa::StopKind::Success, "a small shared skeleton runs");
  const Outcome leaves = run(*machine, "leaf_skeleton.");
  mosa_test::check(leaves.limited, "the fail leaves of a shared skeleton count toward the limit");
  const Outcome again = run(*machine, "twice.");
  mosa_test::check(!again.limited && again.kind == mosa::StopKind::Success,
                   "the IR of an earlier goal that GC can free does not stop the next goal");
  auto wide = mosa::Machine::create(4 * k_limit);
  mosa_test::check(wide->load_program(k_program).ok, "load 32 MiB");
  const Outcome cyclic = run(*wide, "cyclic.");
  mosa_test::check(!cyclic.limited && cyclic.kind == mosa::StopKind::Success,
                   "the nodes of a cycle that the copy folds into its entry do not count");
  const Outcome distinct = run(*wide, "cyclic_distinct.");
  mosa_test::check(!distinct.limited && distinct.kind == mosa::StopKind::Success,
                   "the folded nodes of 10000 distinct cycles do not count");
  const Outcome dag_entry = run(*wide, "cyclic_dag.");
  mosa_test::check(!dag_entry.limited && dag_entry.kind == mosa::StopKind::Success,
                   "the copy does not walk the rest of a cycle entry it folds");
  const Outcome late_entry = run(*wide, "cyclic_dag_late.");
  mosa_test::check(!late_entry.limited && late_entry.kind == mosa::StopKind::Success,
                   "the copy does not walk a shared term before the back edge of its cycle");
  const Outcome scc = run(*wide, "cyclic_scc.");
  mosa_test::check(!scc.limited && scc.kind == mosa::StopKind::Success,
                   "the copy refers to every node of a cycle, not only to the target of its back edge");
  check_failed_query_then_directive();
  return mosa_test::failures();
}
