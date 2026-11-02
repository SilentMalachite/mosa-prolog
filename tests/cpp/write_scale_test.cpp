// レビュー M7。リストを書く時間が要素の数に比例する範囲（4 倍の長さで 8 倍 + 30 ms）に収まること。
// 背骨の巡回の入口を先読みする writer が、開いている祖先の尾で止まらないと、要素がみな [x|L] を指す L を
// 書くとき要素ごとに L 全体をなめて 2 乗になる（M7 の Codex レビュー）。答えで名前の付いた尾 T で止まらないと、
// 要素がみな [x|T] の L を書くときも同じになる（2 回目）。
#include "harness.hpp"

#include "mosa/machine.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace {

// back(L, L) は L の要素をみな [x|L] にする。lasso(N, L) は N 個の前置きの後に 3 要素で巡回するリスト。
// same(L, E) は L の要素をみな E にする。問い合わせで組んだ [x|T] の尾は T の REF なので、答えは [x|T] と書く。
constexpr std::string_view k_program =
    "back([], _).\n"
    "back([[x|L]|T], L) :- back(T, L).\n"
    "plain([]).\n"
    "plain([x|T]) :- plain(T).\n"
    "lasso(N, L) :- length(P, N), plain(P), C = [a, b, c|C], append(P, C, L).\n"
    "append([], L, L).\n"
    "append([H|T], L, [H|R]) :- append(T, L, R).\n"
    "wb(N) :- length(L, N), back(L, L), with_output_to(atom(_), write(L)).\n"
    "wp(N) :- length(L, N), plain(L), with_output_to(atom(_), write(L)).\n"
    "wl(N) :- lasso(N, L), with_output_to(atom(_), write(L)).\n"
    "same([], _).\n"
    "same([E|R], E) :- same(R, E).\n";

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;

// 1 回目は割り当てや GC の始まりを含みうる。最小を取り、偶然の遅れで落ちないようにする。
// answer が真なら、トップレベルと同じく問い合わせの変数の名前で答えを書く時間も含める。
double time_goal(mosa::Machine& machine, const std::string& goal, bool answer) {
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) {
    const auto started = std::chrono::steady_clock::now();
    const auto stop = machine.query(goal);
    if (answer) mosa_test::check(!mosa::format_answer(machine, stop).empty(), goal + " has an answer");
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(stop.kind == mosa::StopKind::Success, goal + " succeeds");
    best = std::min(best, took.count());
  }
  return best;
}

std::string call_of(std::string_view name, std::size_t n) {
  return std::string{name} + "(" + std::to_string(n) + ").";
}

void check_linear(std::string_view label, std::string_view name, std::size_t small_n, bool answer = false) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, std::string{label} + ": load");
  const auto goal = [&](std::size_t n) {
    if (!answer) return call_of(name, n);
    const std::string count = std::to_string(n);
    return "length(T, " + count + "), plain(T), length(L, " + count + "), same(L, [x|T]).";
  };
  const double small = time_goal(*machine, goal(small_n), answer);
  const double large = time_goal(*machine, goal(small_n * 4), answer);
  std::ostringstream text;
  text << std::fixed << std::setprecision(1) << label << ": t(" << small_n * 4 << ") = " << large << " ms, t("
       << small_n << ") = " << small << " ms, ratio " << (small > 0 ? large / small : 0.0) << " (limit 8 x + "
       << k_slack_ms << " ms)";
  // 通っても余裕が見えるよう、測った値を出す。ctest -V で読める。
  std::cout << text.str() << '\n';
  mosa_test::check(large <= 8 * small + k_slack_ms, text.str());
}

}  // namespace

int main() {
  check_linear("elements pointing back", "wb", 5000);
  check_linear("plain list", "wp", 50000);
  check_linear("lasso list", "wl", 50000);
  check_linear("answer with a named tail", "same", 10000, true);
  return mosa_test::failures();
}
