// レビュー M3（ADR 0015）。assertz で積んだ動的述語 p/2 の節を 4 倍にしても、ゴールの時間が節の数に
// 比例する範囲（8 倍 + 30 ms）に収まること。選択点の候補を一歩ごとに写すと、やり直しが節の数の 2 乗になる。
// 測るのはゴールだけ。毎回 mk(N) で p/2 を作り直す（節を足すゴールは mk(0) で空にする）ので、節を変えるゴールも
// 同じ形で足せる。
#include "harness.hpp"

#include "mosa/machine.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace {

// 素の mosa に between/3 は無い。節を積む輪は自前で回す。
constexpr std::string_view k_program =
    ":- dynamic(p/2).\n"
    "mk(N) :- retractall(p(_, _)), mk(1, N).\n"
    "mk(I, N) :- I =< N, !, assertz(p(I, I)), I1 is I + 1, mk(I1, N).\n"
    "mk(_, _).\n"
    "mka(N) :- mka(1, N).\n"
    "mka(I, N) :- I =< N, !, asserta(p(I, I)), I1 is I + 1, mka(I1, N).\n"
    "mka(_, _).\n"
    "one :- p(_, _), !.\n"
    "ones(N) :- ones(1, N).\n"
    "ones(I, N) :- I =< N, !, one, I1 is I + 1, ones(I1, N).\n"
    "ones(_, _).\n"
    "looks(N) :- looks(1, N).\n"
    "looks(I, N) :- I =< N, !, p(I, _), I1 is I + 1, looks(I1, N).\n"
    "looks(_, _).\n"
    "rks(N) :- rks(1, N).\n"
    "rks(I, N) :- I =< N, !, retract(p(I, _)), I1 is I + 1, rks(I1, N).\n"
    "rks(_, _).\n"
    "raks(N) :- raks(1, N).\n"
    "raks(I, N) :- I =< N, !, retractall(p(I, _)), I1 is I + 1, raks(I1, N).\n"
    "raks(_, _).\n"
    "cks(N) :- cks(1, N).\n"
    "cks(I, N) :- I =< N, !, clause(p(I, _), true), I1 is I + 1, cks(I1, N).\n"
    "cks(_, _).\n"
    // retract の選択点は外さずに残す。N 個積もっても、節を消すたびにそれを見てはいけない（todo の待ち行列の形）。
    "rfs(N) :- rfs(1, N).\n"
    "rfs(I, N) :- I =< N, !, retract(p(_, _)), I1 is I + 1, rfs(I1, N).\n"
    "rfs(_, _).\n"
    "rf1 :- retract(p(_, _)), !.\n"
    "rfcs(N) :- rfcs(1, N).\n"
    "rfcs(I, N) :- I =< N, !, rf1, I1 is I + 1, rfcs(I1, N).\n"
    "rfcs(_, _).\n"
    // 節の数の 1/4 から 12 おきに、真ん中の節を消して末尾へ足し直し、鍵で引く。
    "upd(N) :- K is N // 25, upd(0, K, N).\n"
    "upd(J, K, _) :- J >= K, !.\n"
    "upd(J, K, N) :- I is N // 4 + J * 12, retract(p(I, _)), assertz(p(I, x)), p(7, _), J1 is J + 1, upd(J1, K, N).\n";

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;

using GoalOf = std::function<std::string(std::size_t)>;

std::string build_full(std::size_t n) { return "mk(" + std::to_string(n) + ")."; }

// 1 回目は割り当てや GC の始まりを含みうる。最小を取り、偶然の遅れで落ちないようにする。
double time_goal(mosa::Machine& machine, std::string_view label, std::size_t n, const GoalOf& goal_of,
                 const GoalOf& build_of) {
  const std::string build = build_of(n);
  const std::string goal = goal_of(n);
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) {
    const auto built = machine.query(build);
    mosa_test::check(built.kind == mosa::StopKind::Success, std::string{label} + ": " + build);
    const auto started = std::chrono::steady_clock::now();
    const auto stop = machine.query(goal);
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(stop.kind == mosa::StopKind::Success, std::string{label} + ": " + goal + " succeeds");
    best = std::min(best, took.count());
  }
  return best;
}

// build_of は計る前に p/2 を作り直すゴール。節を足すゴールを計るときは空にする（mk(0)）。
void check_linear(std::string_view label, std::size_t small_n, const GoalOf& goal_of,
                  const GoalOf& build_of = build_full) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, std::string{label} + ": load");
  const double small = time_goal(*machine, label, small_n, goal_of, build_of);
  const double large = time_goal(*machine, label, small_n * 4, goal_of, build_of);
  std::ostringstream text;
  text << std::fixed << std::setprecision(1) << label << ": t(" << small_n * 4 << ") = " << large << " ms, t("
       << small_n << ") = " << small << " ms, ratio " << (small > 0 ? large / small : 0.0) << " (limit 8 x + "
       << k_slack_ms << " ms)";
  // 通っても余裕が見えるよう、測った値を出す。ctest -V で読める。
  std::cout << text.str() << '\n';
  mosa_test::check(large <= 8 * small + k_slack_ms, text.str());
}

// N に依らないゴールは、そのまま渡す。
void check_linear(std::string_view label, std::size_t small_n, std::string_view goal) {
  check_linear(label, small_n, [goal](std::size_t) { return std::string{goal}; });
}

}  // namespace

int main() {
  check_linear("scan", 25000, "(p(_, _), fail ; true).");
  // 先頭へ足すたびに後ろの節を全部ずらすか、assert のたびに死んだ節を探して全部の節をなめると、N 回の asserta が
  // N の 2 乗になる。
  check_linear(
      "asserta", 10000, [](std::size_t n) { return "mka(" + std::to_string(n) + ")."; },
      [](std::size_t) { return std::string{"mk(0)."}; });
  // 第1引数が未束縛の呼び出しが生きた節を全部候補の列に積むと、すぐカットしても 1 回が N かかり、N 回で N の 2 乗になる。
  check_linear("unbound first call", 5000, [](std::size_t n) { return "ones(" + std::to_string(n) + ")."; });
  // 第1引数で引く呼び出しが全部の節の鍵を照らすと、鍵が 1 節にしか合わなくても 1 回が N かかり、N 回で N の 2 乗になる。
  check_linear("lookup by key", 5000, [](std::size_t n) { return "looks(" + std::to_string(n) + ")."; });
  // retract・retractall・clause/2 が全部の節を試しに単一化すると、鍵が 1 節にしか合わなくても 1 回が N かかる。
  check_linear("retract by key", 2500, [](std::size_t n) { return "rks(" + std::to_string(n) + ")."; });
  check_linear("retractall by key", 2500, [](std::size_t n) { return "raks(" + std::to_string(n) + ")."; });
  check_linear("clause/2 by key", 2500, [](std::size_t n) { return "cks(" + std::to_string(n) + ")."; });
  // 第1引数が未束縛の retract と clause/2 の走査。やり直しのたびに節を集め直すか写すと、N の 2 乗になる。
  check_linear("retract scan", 2500, "(retract(p(_, _)), fail ; true).");
  check_linear("clause/2 scan", 2500, "(clause(p(_, _), true), fail ; true).");
  // 先頭の 1 節だけが要る retract が全部の節を集めると、1 回が N かかる。選択点を残したままの retract は、消すたびに
  // 選択点を全部見ても N の 2 乗になる。カットで外す形も足す。
  check_linear("retract from front", 5000, [](std::size_t n) { return "rfs(" + std::to_string(n) + ")."; });
  check_linear("retract from front, cut", 2500, [](std::size_t n) { return "rfcs(" + std::to_string(n) + ")."; });
  // 真ん中の節を消すたびに索引を捨てると、次の鍵の呼び出しが索引を作り直し、1 回が N かかる（Ruling 10）。
  check_linear("update by key", 2500, [](std::size_t n) { return "upd(" + std::to_string(n) + ")."; });
  return mosa_test::failures();
}
