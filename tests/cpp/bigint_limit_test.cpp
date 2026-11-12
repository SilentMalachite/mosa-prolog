// フェーズ10（ADR 0025）。Big の算術はヒープの上限で止まる。+ - * は結果の大きさを計算の前に見積もり（check_room）、
// 式の途中で置き場に持つ Big の写しも上限に数える（from_big、レビュー Important 1）。越えるなら計算も確保もせずに
// ResourceLimit("heap_limit") を投げる（タスク 6）。huge_mul は止まることだけを見る。見積もりがないと、次の 2 乗
// （上限の約 0.65 倍）をヒープの外で計算してから from_big で止まる。積を箱にせずに止めることは product_estimate で確かめる。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;
// product_estimate の機械（327,680 語）。A（約 85k 語）と葉の写し 2 つ（約 170k 語）は入り、A * A（約 170k 語）は入らない。
constexpr std::size_t k_small = std::size_t{5} << 16;

constexpr std::string_view k_program =
    // ^ の Big はタスク 7 なので、10^400 は * だけで作る。
    "pow10(0, 1) :- !.\n"
    "pow10(N, X) :- M is N - 1, pow10(M, Y), X is Y * 10.\n"
    // 2 乗を限りなく続ける。語の数は 1 歩ごとに倍になる。
    "sq(X) :- Y is X * X, sq(Y).\n"
    // sqn(12, 10^400, A) の A は約 85k 語。途中の 2 乗の箱は garbage_collect で消える。
    "sqn(0, X, X) :- !.\n"
    "sqn(N, X, Y) :- M is N - 1, Z is X * X, sqn(M, Z, Y).\n"
    // max(A, max(A, ... 0)) の深さ N の入れ子。ヒープでは 1 段に数語だが、評価は段ごとに A の写しを持つ。
    "mnest(0, _, 0) :- !.\n"
    "mnest(N, A, max(A, E)) :- M is N - 1, mnest(M, A, E).\n"
    // ((0 + A) + A) + ... の深さ N の入れ子。段ごとに途中の和を settle_bigs が寄せ、写しは 3 つほどで済む。
    "lnest(0, _, 0) :- !.\n"
    "lnest(N, A, E + A) :- M is N - 1, lnest(M, A, E).\n";

constexpr std::string_view k_big_a = "pow10(400, P), sqn(12, P, A), garbage_collect, ";

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

// 葉の写しは上限に数える。深さ 200 では写しが上限の約 8 倍になる。max は check_room を通らず、比較で終えるので
// num_cell の箱でも止まらない。深さ 200 は eval_deep を、深さ 8 は eval_at を通る。
void check_nested_hold() {
  for (const auto& [depth, limited] : {std::pair{200, true}, std::pair{8, false}}) {
    auto machine = mosa::Machine::create(k_limit);
    mosa_test::check(machine->load_program(k_program).ok, "nested_hold: load");
    const std::string goal = std::string{k_big_a} + "mnest(" + std::to_string(depth) + ", A, E), E > 0.";
    const Outcome outcome = run(*machine, goal);
    const std::string label = "nested_hold: depth " + std::to_string(depth);
    if (limited) {
      mosa_test::check(outcome.limited, label + " stops at the heap limit");
    } else {
      mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, label + " fits");
    }
  }
}

// settle_bigs が消した写しの語を引かないと、勘定は段ごとに A の分だけ増え、十数段で上限に届く。深さ 200 の
// 左の入れ子の和が入ることで、勘定の数え過ぎがないことを確かめる（レビューの再確認 Minor 3）。
void check_settle_count() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "settle_count: load");
  const Outcome outcome = run(*machine, std::string{k_big_a} + "lnest(200, A, E), E > 0.");
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, "settle_count: depth 200 sum fits");
}

// A * A は葉の写し 2 つの後に約 170k 語を要り、上限を越える。比較で終えるので、num_cell の箱では止まらない。
// A >= A は同じ写し 2 つを持つだけで入るので、止まる理由が積であることを示す。
void check_product_estimate() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_program).ok, "product_estimate: load");
  const Outcome both = run(*machine, std::string{k_big_a} + "A >= A.");
  mosa_test::check(!both.limited && both.kind == mosa::StopKind::Success, "product_estimate: A >= A fits");
  const Outcome product = run(*machine, std::string{k_big_a} + "A * A > 0.");
  mosa_test::check(product.limited, "product_estimate: A * A > 0 stops at the heap limit");
}

}  // namespace

int main() {
  check_nested_hold();
  check_settle_count();
  check_product_estimate();
  check_huge_mul();
  return mosa_test::failures();
}
