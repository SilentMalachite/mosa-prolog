// レビュー M25（ADR 0018）。述語の数を 4 倍にしても、決まった数の呼び出し、やり直し、abolish の時間は変わらず
// （2 倍 + 30 ms の範囲）、P 個の 1 節の述語の consult、assertz、後ろからの abolish は P に比例する範囲（8 倍 +
// 30 ms）に収まること。名前で述語を引くたびに表をなめると、呼び出しは述語の数に比例し、consult と assertz は述語の
// 数の 2 乗になる。
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

// 素の mosa に between/3 は無い。輪は自前で回す。loop2 は 2 節の t2 を呼んでやり直す。mkp は p0/1 から順に
// 新しい述語を assertz で作り、abb は後ろから abolish する。cyc は 1 つの述語を abolish して作り直す。
constexpr std::string_view k_rules =
    "loop(0) :- !.\n"
    "loop(N) :- t, M is N - 1, loop(M).\n"
    "t.\n"
    "loop2(0) :- !.\n"
    "loop2(N) :- (t2, fail ; true), M is N - 1, loop2(M).\n"
    "t2.\n"
    "t2.\n"
    "mkp(N, N) :- !.\n"
    "mkp(I, N) :- number_codes(I, Cs), atom_codes(A, [0'p|Cs]), T =.. [A, I], assertz(T), J is I + 1, mkp(J, N).\n"
    "abb(0) :- !.\n"
    "abb(I) :- J is I - 1, number_codes(J, Cs), atom_codes(A, [0'p|Cs]), abolish(A/1), abb(J).\n"
    "cyc(0) :- !.\n"
    "cyc(K) :- abolish(q/1), assertz(q(K)), K1 is K - 1, cyc(K1).\n";

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;
constexpr std::string_view k_calls = "50000";

std::string facts(std::size_t count) {
  std::string text;
  for (std::size_t at = 0; at < count; ++at) text += "f" + std::to_string(at) + "(" + std::to_string(at) + ").\n";
  return text;
}

double elapsed_ms(std::chrono::steady_clock::time_point started) {
  const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
  return took.count();
}

// Consult は count 個の事実の読み込み、Calls と Retries はその隣の決まった数の呼び出しとやり直し、Asserts は
// count 個の新しい述語の assertz、Abolish はそれを後ろから abolish する時間、Churn は count 個を作って全部消した
// 後の、決まった数の abolish と assertz を測る。
enum class Measure { Consult, Calls, Retries, Asserts, Abolish, Churn };

std::string goal_of(Measure what, std::size_t count) {
  if (what == Measure::Calls) return "loop(" + std::string{k_calls} + ").";
  if (what == Measure::Retries) return "loop2(" + std::string{k_calls} + ").";
  if (what == Measure::Abolish) return "abb(" + std::to_string(count) + ").";
  if (what == Measure::Churn) return "cyc(20000).";
  return "mkp(0, " + std::to_string(count) + ").";
}

// 測る前に、測らずに通すゴール。
std::string setup_of(Measure what, std::size_t count) {
  if (what == Measure::Abolish) return "mkp(0, " + std::to_string(count) + ").";
  if (what == Measure::Churn) return "mkp(0, " + std::to_string(count) + "), abb(" + std::to_string(count) + ").";
  return "true.";
}

// 1 回目は割り当ての始まりを含みうる。毎回新しい機械で測り、最小を取る。
double time_once(std::size_t count, Measure what, std::string_view label) {
  auto machine = mosa::Machine::create();
  const bool with_facts = what == Measure::Consult || what == Measure::Calls || what == Measure::Retries;
  const std::string program = (with_facts ? facts(count) : std::string{}) + std::string{k_rules};
  const auto started = std::chrono::steady_clock::now();
  mosa_test::check(machine->load_program(program).ok, std::string{label} + ": load");
  if (what == Measure::Consult) return elapsed_ms(started);
  const std::string setup = setup_of(what, count);
  mosa_test::check(machine->query(setup).kind == mosa::StopKind::Success, std::string{label} + ": " + setup);
  const std::string goal = goal_of(what, count);
  const auto began = std::chrono::steady_clock::now();
  const auto stop = machine->query(goal);
  const double took = elapsed_ms(began);
  mosa_test::check(stop.kind == mosa::StopKind::Success, std::string{label} + ": " + goal + " succeeds");
  return took;
}

double best_of(std::size_t count, Measure what, std::string_view label) {
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) best = std::min(best, time_once(count, what, label));
  return best;
}

void check_scale(std::string_view label, std::size_t small_n, Measure what, double factor) {
  const double small = best_of(small_n, what, label);
  const double large = best_of(small_n * 4, what, label);
  std::ostringstream text;
  text << std::fixed << std::setprecision(1) << label << ": t(" << small_n * 4 << ") = " << large << " ms, t("
       << small_n << ") = " << small << " ms, ratio " << (small > 0 ? large / small : 0.0) << " (limit " << factor
       << " x + " << k_slack_ms << " ms)";
  // 通っても余裕が見えるよう、測った値を出す。ctest -V で読める。
  std::cout << text.str() << '\n';
  mosa_test::check(large <= factor * small + k_slack_ms, text.str());
}

}  // namespace

int main() {
  // 2 乗の項が比例の項を上回るよう 1 万個から測る。5,000 個では、表をなめる 1ee2e8b でも比は 10 前後に留まった。
  check_scale("consult of one-clause predicates", 10000, Measure::Consult, 8.0);
  check_scale("assertz of new predicates", 10000, Measure::Asserts, 8.0);
  check_scale("calls next to many predicates", 5000, Measure::Calls, 2.0);
  check_scale("retries next to many predicates", 5000, Measure::Retries, 2.0);
  // 後ろの述語を消すたびに索引の全部の枠をなめると、P 個を後ろから消すのが P の 2 乗になる。枠は縮めないので、
  // 多くの述語を消した後の abolish も、一番大きかった時の枠の数だけかかる（M25 の敵対的レビュー B1、B2）。
  check_scale("abolish from the back", 10000, Measure::Abolish, 8.0);
  check_scale("abolish after many predicates were erased", 10000, Measure::Churn, 2.0);
  return mosa_test::failures();
}
