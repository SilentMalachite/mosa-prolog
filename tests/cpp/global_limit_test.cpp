// レビュー 2026-11-20 の H-03。nb_setval/2 の値は問い合わせの後も残るので、その語は Heap::held に数え続ける。前は
// 新しい値を置き、古い値の語を返してから数えたので、上限で投げても新しい値と持った語が残った。持った語だけで上限を
// 超えると、その後は最初の安全点ですべての問い合わせが止まり、nb_setval(k, small) でも戻せなかった。今は置く前に
// 確かめ、超えるなら古い値も持った語も変えずに 1 度だけ止める。
// 上限は 16 MiB（2^21 語）。変数だけのリストは外の写しで 1 要素 200 バイトほどに数えるので、10 万要素の値は上限を超え、
// 2 万要素なら収まる。どちらもヒープでは上限の 1/4 に届かない。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;

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

void check_runs(mosa::Machine& machine, std::string_view goal) {
  const Outcome outcome = run(machine, goal);
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, std::string{goal} + ": succeeds");
}

// 止まった後も持った語は前のままで、同じ機械で次の問い合わせが動く。
void check_stops(mosa::Machine& machine, std::string_view goal) {
  const std::size_t held = machine.heap().held();
  mosa_test::check(run(machine, goal).limited, std::string{goal} + ": stops at the heap limit");
  mosa_test::check(machine.heap().held() == held, std::string{goal} + ": held words unchanged");
  check_runs(machine, "X = f(Y), Y = 1, X == f(1).");
}

}  // namespace

int main() {
  auto machine = mosa::Machine::create(k_limit);
  check_runs(*machine, "nb_setval(h03_k, old).");
  const std::size_t held = machine->heap().held();
  // 上限を超える値は置かない。古い値が残る。
  check_stops(*machine, "length(L, 100000), nb_setval(h03_k, L).");
  check_runs(*machine, "nb_getval(h03_k, V), V == old.");
  // 置いていない鍵は、止まった後も置いていないまま。
  check_stops(*machine, "length(L, 100000), nb_setval(h03_new, L).");
  check_runs(*machine, "catch((nb_getval(h03_new, _), fail), error(existence_error(variable, h03_new), _), true).");
  // 守り。収まる値は前と同じく置けて読み戻せ、小さな値に替えれば語を返す。
  check_runs(*machine, "length(L, 20000), nb_setval(h03_k, L).");
  mosa_test::check(machine->heap().held() > held, "a fitting value is held");
  check_runs(*machine, "nb_getval(h03_k, V), length(V, 20000).");
  check_runs(*machine, "nb_setval(h03_k, small).");
  mosa_test::check(machine->heap().held() == held, "a small value gives the words back");
  return mosa_test::failures();
}
