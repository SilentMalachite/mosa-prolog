// レビュー 2026-11-20 の M-27。setof/3 の並べ替えと bagof/3 の組の引き当てが、要素数に比例する範囲（4 倍の大きさで
// 8 倍 + 30 ms）に収まること。前は setof/3 が挿入ソートで、bagof/3 がどの答えでもすべての組を比べ、どちらも 2 乗だった
// （逆順の 8000 要素の setof が 0.35 秒、8000 の鍵の bagof が 6.9 秒）。時間を測るので、GC ストレスと ASan では流さない。
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

namespace {

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;

constexpr const char* k_program =
    "mem(X, [X|_]).\n"
    "mem(X, [_|T]) :- mem(X, T).\n"
    "upto(N, N, [N]) :- !.\n"
    "upto(I, N, [I|T]) :- J is I + 1, upto(J, N, T).\n"
    "pairs([], []).\n"
    "pairs([K|Ks], [K-w(K)|Ps]) :- pairs(Ks, Ps).\n"
    "rev(L, R) :- rev(L, [], R).\n"
    "rev([], A, A).\n"
    "rev([H|T], A, R) :- rev(T, [H|A], R).\n"
    "desc(N, L) :- upto(1, N, L0), rev(L0, L).\n";

double time_goal(mosa::Machine& machine, const std::string& goal) {
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) {
    const auto started = std::chrono::steady_clock::now();
    const auto stop = machine.query(goal);
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(stop.kind == mosa::StopKind::Success, "goal succeeds: " + goal.substr(0, 60));
    best = std::min(best, took.count());
  }
  return best;
}

void check_linear(const char* label, const std::string& goal_of_n) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, "load the program");
  const auto goal = [&goal_of_n](int n) {
    std::string text = goal_of_n;
    for (std::size_t at = text.find('N'); at != std::string::npos; at = text.find('N', at)) {
      text.replace(at, 1, std::to_string(n));
    }
    return text;
  };
  const double small = time_goal(*machine, goal(2000));
  const double large = time_goal(*machine, goal(8000));
  std::ostringstream text;
  text << std::fixed << std::setprecision(1) << label << ": large = " << large << " ms, small = " << small
       << " ms, ratio " << (small > 0 ? large / small : 0.0) << " (limit 8 x + " << k_slack_ms << " ms)";
  std::cout << text.str() << '\n';
  mosa_test::check(large <= 8 * small + k_slack_ms, text.str());
}

}  // namespace

int main() {
  // 逆順は挿入ソートの最悪。値を並べ替えて重なりを除くのも見る。
  check_linear("setof descending", "desc(N, L), setof(X, mem(X, L), S), length(S, N).");
  // 鍵ごとに 1 つの組。昇順でも降順でも、前はどの答えでもすべての組を比べた。
  check_linear("bagof keys descending",
               "desc(N, L), pairs(L, P), findall(K-Xs, bagof(X, mem(K-X, P), Xs), R), length(R, N).");
  check_linear("bagof keys ascending",
               "upto(1, N, L), pairs(L, P), findall(K-Xs, bagof(X, mem(K-X, P), Xs), R), length(R, N).");
  // 鍵が複合項 w(K) でも同じ（目印は Share を辿って前から数える）。
  check_linear("setof compound keys",
               "desc(N, L), pairs(L, P), findall(W-Ks, setof(K, mem(K-W, P), Ks), R), length(R, N).");
  return mosa_test::failures();
}
