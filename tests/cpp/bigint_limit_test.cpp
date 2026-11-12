// フェーズ10（ADR 0025）。Big の算術はヒープの上限で止まる。結果の大きさを計算の前に見積もり、ヒープの残りを
// 越えるなら計算も確保もせずに ResourceLimit("heap_limit") を投げる（タスク 6）。上限は 16 MiB（2^21 語）。
// 見積もらなければ、2 乗を続けるループはヒープの外の BigInt を上限の何倍にも育ててから、箱にするときに止まる。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;

constexpr std::string_view k_program =
    // ^ の Big はタスク 7 なので、10^400 は * だけで作る。
    "pow10(0, 1) :- !.\n"
    "pow10(N, X) :- M is N - 1, pow10(M, Y), X is Y * 10.\n"
    // 2 乗を限りなく続ける。語の数は 1 歩ごとに倍になる。
    "sq(X) :- Y is X * X, sq(Y).\n";

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

void check_huge_mul() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "huge_mul: load");
  const Outcome outcome = run(*machine, "pow10(400, A), sq(A).");
  mosa_test::check(outcome.limited, "huge_mul: the squares stop at the heap limit");
  // 止まった後も同じ機械で次の問い合わせが動く。
  const Outcome after = run(*machine, "X is 9223372036854775807 * 4, X > 0.");
  mosa_test::check(!after.limited && after.kind == mosa::StopKind::Success, "huge_mul: the machine runs again");
}

}  // namespace

int main() {
  check_huge_mul();
  return mosa_test::failures();
}
