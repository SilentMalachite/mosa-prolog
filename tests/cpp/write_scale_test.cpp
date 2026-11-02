// レビュー M7。リストを書く時間が要素の数に比例する範囲（4 倍の長さで 8 倍 + 30 ms）に収まること。
// 背骨の巡回の入口を先読みする writer が、開いている祖先の尾で止まらないと、要素がみな [x|L] を指す L を
// 書くとき要素ごとに L 全体をなめて 2 乗になる（M7 の Codex レビュー）。
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
    "wl(N) :- lasso(N, L), with_output_to(atom(_), write(L)).\n";

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;

// 1 回目は割り当てや GC の始まりを含みうる。最小を取り、偶然の遅れで落ちないようにする。
double time_goal(mosa::Machine& machine, std::string_view name, std::size_t n) {
  const std::string goal = std::string{name} + "(" + std::to_string(n) + ").";
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) {
    const auto started = std::chrono::steady_clock::now();
    const auto stop = machine.query(goal);
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(stop.kind == mosa::StopKind::Success, goal + " succeeds");
    best = std::min(best, took.count());
  }
  return best;
}

void check_linear(std::string_view label, std::string_view name, std::size_t small_n) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, std::string{label} + ": load");
  const double small = time_goal(*machine, name, small_n);
  const double large = time_goal(*machine, name, small_n * 4);
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
  return mosa_test::failures();
}
