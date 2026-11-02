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
    "ones(_, _).\n";

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
  return mosa_test::failures();
}
