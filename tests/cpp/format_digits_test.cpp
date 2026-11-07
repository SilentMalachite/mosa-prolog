// レビュー M29。'$format_float'/4（format/2 の ~e と ~f）は、どの桁数でもプロセスを終わらせない。
// 有限の数で桁数がヒープの上限の語数を超えれば、format.pl が作る字のリストは必ず上限を超えるので、
// ヒープの外に文字列を作る前に上限で止まる。旧コードは 2^63 - 1 桁で length_error が外に漏れてプロセスごと落ち、
// 上限を少し超える桁は上限の外に原子を作って成功した。無限大と NaN は桁数によらず writeq と同じ字面になる。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace {

// 8 MiB（2^20 語）。
constexpr std::size_t k_limit = std::size_t{1} << 20;

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

void check_stops(std::string_view label, std::string_view goal) {
  auto machine = mosa::Machine::create(k_limit);
  const Outcome outcome = run(*machine, goal);
  mosa_test::check(outcome.limited, std::string{label} + ": stops at the heap limit");
  const Outcome again = run(*machine, "true.");
  mosa_test::check(!again.limited && again.kind == mosa::StopKind::Success,
                   std::string{label} + ": the machine runs again after the stop");
}

void check_succeeds(std::string_view label, std::string_view goal) {
  auto machine = mosa::Machine::create(k_limit);
  const Outcome outcome = run(*machine, goal);
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, std::string{label} + ": succeeds");
}

}  // namespace

int main() {
  check_stops("e max", "'$format_float'(e, 9223372036854775807, 1.0, _).");
  check_stops("f max", "'$format_float'(f, 9223372036854775807, -2.5, _).");
  check_stops("e above", "'$format_float'(e, 1048577, 1.0, _).");
  check_stops("f above", "'$format_float'(f, 1048577, 0.5, _).");
  // 上限の語数ちょうどの桁は作る。
  check_succeeds("e at", "'$format_float'(e, 1048576, 1.0, A), atom_length(A, 1048582).");
  check_succeeds("f at", "'$format_float'(f, 1048576, 0.5, A), atom_length(A, 1048578).");
  check_succeeds("inf max", "X is inf, '$format_float'(e, 9223372036854775807, X, A), A == '1.0Inf'.");
  check_succeeds("-inf max", "X is -inf, '$format_float'(f, 9223372036854775807, X, A), A == '-1.0Inf'.");
  check_succeeds("nan max", "X is -(nan), '$format_float'(e, 9223372036854775807, X, A), A == '1.5NaN'.");
  return mosa_test::failures();
}
