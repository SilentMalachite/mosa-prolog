// 全体レビュー推奨4。=../2 のリスト読み、読み手の変数表、書き手の名前引きが要素数に比例する範囲
// （4 倍の大きさで 8 倍 + 30 ms）に収まること。前はどれも線形走査の入れ子で 2 乗だった。
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

double time_goal(mosa::Machine& machine, const std::string& goal, bool answer) {
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) {
    const auto started = std::chrono::steady_clock::now();
    const auto stop = machine.query(goal);
    if (answer) mosa_test::check(!mosa::format_answer(machine, stop).empty(), "answer written");
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(stop.kind == mosa::StopKind::Success, "goal succeeds: " + goal.substr(0, 40));
    best = std::min(best, took.count());
  }
  return best;
}

void check_linear(const char* label, const std::string& small_goal, const std::string& large_goal, bool answer) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("append([], L, L).\nappend([H|T], L, [H|R]) :- append(T, L, R).\n").ok,
                   "load append");
  const double small = time_goal(*machine, small_goal, answer);
  const double large = time_goal(*machine, large_goal, answer);
  std::ostringstream text;
  text << std::fixed << std::setprecision(1) << label << ": large = " << large << " ms, small = " << small
       << " ms, ratio " << (small > 0 ? large / small : 0.0) << " (limit 8 x + " << k_slack_ms << " ms)";
  std::cout << text.str() << '\n';
  mosa_test::check(large <= 8 * small + k_slack_ms, text.str());
}

std::string univ_goal(std::size_t n) {
  return "length(L, " + std::to_string(n) +
         "), catch((_ =.. [f|L], fail), error(representation_error(max_arity), _), true).";
}

std::string cyclic_goal(std::size_t n) {
  return "length(P, " + std::to_string(n) +
         "), C = [x|C], append(P, C, L), catch((_ =.. [f|L], fail), error(type_error(list, _), _), true).";
}

std::string var_list(std::size_t n) {
  std::string text = "[";
  for (std::size_t index = 0; index < n; ++index) {
    if (index > 0) text += ",";
    text += "V" + std::to_string(index);
  }
  return text + "]";
}

}  // namespace

int main() {
  check_linear("univ long list", univ_goal(20000), univ_goal(80000), false);
  check_linear("univ cyclic spine", cyclic_goal(20000), cyclic_goal(80000), false);
  check_linear("reader variables", "length(" + var_list(4000) + ", 4000).", "length(" + var_list(16000) + ", 16000).",
               false);
  check_linear("writer variables", "Z = " + var_list(4000) + ".", "Z = " + var_list(16000) + ".", true);
  return mosa_test::failures();
}
