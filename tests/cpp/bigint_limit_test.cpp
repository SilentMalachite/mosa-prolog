// フェーズ10（ADR 0025）。Big の算術はヒープの上限で止まる。+ - * は結果の大きさを計算の前に見積もり（check_room）、
// 式の途中で置き場に持つ Big の写しも上限に数える（from_big、レビュー Important 1）。越えるなら計算も確保もせずに
// ResourceLimit("heap_limit") を投げる（タスク 6）。huge_mul は止まることだけを見る。見積もりがないと、次の 2 乗
// （上限の約 0.65 倍）をヒープの外で計算してから from_big で止まる。積を箱にせずに止めることは product_estimate で確かめる。
// << と ^ も結果の大きさを先に見積もる（タスク 7）。huge_shift はその見積もりだけで、確保の前に止まることを見る。
// 整数の引数の Big（タスク 8）。length/2 の正の Big の長さと、'$format_float'/4 の正の Big の桁数は、どのヒープにも
// 入らないので、作り始める前に止まる（length_big）。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <chrono>
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
    // 10^400 は * だけで作る。積の見積もり（check_room）の行が ^ の見積もりに頼らない。
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

// 1 << 2^62 と 2^(2^62) は 2^62 ビット（約 2^56 語）を越える。見積もりがなければ、shift_left と power が
// 結果の語を確保しようとして std::bad_alloc か std::length_error を投げるか、長い時間をかける。見積もりで止まれば
// 計算も確保もしないので、1 秒の中で止まり、ヒープの確保も伸びない。シフトの数と指数が Big の 2 行は、見積もらずに
// その場で止まる。(2^127)^(2^62) は見積もりの積 128 * 2^62 が 2^64 で 0 に折り返すので、張り付けなければ
// power が走る。
void check_huge_shift() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "huge_shift: load");
  // 最初の問い合わせで確保が伸びても、比べる前に済ませておく。
  const Outcome warm = run(*machine, "X is 1 << 62, X > 0.");
  mosa_test::check(!warm.limited && warm.kind == mosa::StopKind::Success, "huge_shift: warm up");
  for (const std::string_view goal : {"X is 1 << (1 << 62).", "X is 2^(2^62).", "X is 1 << (2^70).", "X is 3^(2^70).",
                                      "X is (1 << 127)^(1 << 62)."}) {
    const std::string label = "huge_shift: " + std::string{goal};
    const std::size_t capacity = machine->heap().capacity();
    const auto started = std::chrono::steady_clock::now();
    const Outcome outcome = run(*machine, goal);
    const std::chrono::duration<double> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(outcome.limited, label + " stops at the heap limit");
    mosa_test::check(took.count() < 1.0, label + " stops within 1 s (" + std::to_string(took.count()) + " s)");
    mosa_test::check(machine->heap().capacity() == capacity, label + " leaves the heap capacity as it was");
  }
  const Outcome after = run(*machine, "X is 2^100, X > 0.");
  mosa_test::check(!after.limited && after.kind == mosa::StopKind::Success, "huge_shift: the machine runs again");
}

// 大きすぎる int64 の長さは変数を作り続けて上限で止まり、ヒープを上限まで伸ばす。正の Big は作り始める前に止まる
// ので、1 秒の中で止まり、ヒープの確保も伸びない。閉じたリストの長さとは合わないので、そちらは失敗する。
void check_length_big() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "length_big: load");
  const Outcome warm = run(*machine, "length(L, 3), L = [_|_].");
  mosa_test::check(!warm.limited && warm.kind == mosa::StopKind::Success, "length_big: warm up");
  for (const std::string_view goal : {"length(L, 1844674407370909797907654848955145546336677616).",
                                      "length([a, b|T], 1844674407370909797907654848955145546336677616).",
                                      "'$format_float'(f, 1844674407370909797907654848955145546336677616, 1.0, A)."}) {
    const std::string label = "length_big: " + std::string{goal};
    const std::size_t capacity = machine->heap().capacity();
    const auto started = std::chrono::steady_clock::now();
    const Outcome outcome = run(*machine, goal);
    const std::chrono::duration<double> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(outcome.limited, label + " stops at the heap limit");
    mosa_test::check(took.count() < 1.0, label + " stops within 1 s (" + std::to_string(took.count()) + " s)");
    mosa_test::check(machine->heap().capacity() == capacity, label + " leaves the heap capacity as it was");
  }
  const Outcome closed = run(*machine, "length([a], 1844674407370909797907654848955145546336677616).");
  mosa_test::check(!closed.limited && closed.kind == mosa::StopKind::Failure, "length_big: a closed list fails");
  // 無限大は桁数を使わないので、Big の桁数でも止まらずに 1 つの字面になる。
  const Outcome infinite =
      run(*machine, "X is inf, '$format_float'(f, 1844674407370909797907654848955145546336677616, X, A), A == '1.0Inf'.");
  mosa_test::check(!infinite.limited && infinite.kind == mosa::StopKind::Success, "length_big: infinity ignores digits");
  const Outcome after = run(*machine, "length(L, 2), L = [_, _].");
  mosa_test::check(!after.limited && after.kind == mosa::StopKind::Success, "length_big: the machine runs again");
}

}  // namespace

int main() {
  check_length_big();
  check_huge_shift();
  check_nested_hold();
  check_settle_count();
  check_product_estimate();
  check_huge_mul();
  return mosa_test::failures();
}
