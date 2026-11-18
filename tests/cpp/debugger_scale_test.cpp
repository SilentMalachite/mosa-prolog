// PR #4 の敵対的レビュー（ADR 0028）。トレースとスパイポイントの leap で、再帰の時間が回数に比例する範囲（4 倍の回数で
// 8 倍 + 30 ms）に収まること。選択点を残す再帰の中のスパイポイントも同じ。ポートの行の深さをポートのたびにスタックの
// 箱を数え直して出すと、末尾呼び出しでも残る箱の数だけかかり、回数の 2 乗になる（手元で leap の cnt(40000) が 2.8 秒）。
// ポートの行は書くだけで止まらない口に渡す。
#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace {

// cnt/1 は末尾再帰、len/2 は末尾でない再帰。リストはトレースの前に作る。gen/1 は段ごとに選択点を 1 つ残して t を呼ぶ。
constexpr std::string_view k_program =
    "cnt(0) :- !.\n"
    "cnt(K) :- K1 is K - 1, cnt(K1).\n"
    "len([], 0).\n"
    "len([_|T], N) :- len(T, M), N is M + 1.\n"
    "t.\n"
    "gen(0) :- !.\n"
    "gen(K) :- (t ; true), K1 is K - 1, gen(K1).\n";

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;

struct Sink {
  std::size_t lines = 0;
};

// read_line を渡さないので、どのポートも止まらずに行を書くだけで進む（端末でない mosa と同じ）。
std::unique_ptr<mosa::Machine> make_machine(Sink& sink) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, "load cnt/1 and len/2");
  mosa::DebugConsole console;
  console.write = [&sink](std::string_view) { ++sink.lines; };
  mosa::engine_impl(*machine).debug.console = std::move(console);
  return machine;
}

// 1 回目は割り当てや GC の始まりを含みうる。最小を取り、偶然の遅れで落ちないようにする。
double time_goal(mosa::Machine& machine, const std::string& goal) {
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

// goal の中の N を回数に置き換えて測る。setup は測る前に 1 回だけ問う。
void check_linear(std::string_view label, std::string_view setup, std::string_view goal, std::size_t small_n) {
  Sink sink;
  auto machine = make_machine(sink);
  mosa_test::check(machine->query(setup).kind == mosa::StopKind::Success, std::string{label} + ": setup");
  const auto with = [goal](std::size_t n) {
    std::string text{goal};
    text.replace(text.find('N'), 1, std::to_string(n));
    return text;
  };
  const double small = time_goal(*machine, with(small_n));
  const double large = time_goal(*machine, with(small_n * 4));
  mosa_test::check(sink.lines > small_n, std::string{label} + ": the ports were written");
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
  check_linear("trace, tail recursion", "true.", "trace, cnt(N).", 5000);
  check_linear("trace, non-tail recursion", "true.", "length(L, N), trace, len(L, _).", 5000);
  // spy/1 の知らせは user_error に書くので、測る前に 1 回だけ置く。
  check_linear("leap to a spied tail recursion", "spy(cnt/1).", "cnt(N).", 5000);
  // leap では箱が少ない。箱を開くたびに一番近い開いた箱をスタックの下へ探すと、選択点の数だけかかる（PR #4 の 2 度目の
  // 敵対的レビュー。手元で gen(80000) が gen(20000) の 16.6 倍）。
  check_linear("leap to a spied call under choice points", "spy(t/0).", "gen(N).", 5000);
  return mosa_test::failures();
}
