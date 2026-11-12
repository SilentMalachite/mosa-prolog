// 7.9 の式の評価のうち、int64 に入らない整数（Big、ADR 0025）の置き場と遅い道。9.1.3 の + - * // rem div mod、
// 9.1.4 の整数どうしの /、9.1.6 の float からの丸め、8.7 の比較、9.3.10 の ^、9.4 のビット演算とシフト、gcd が Big を
// 受ける。<< と ^ と + - * は、結果の大きさを計算の前に見積もってヒープの上限と比べる。評価の間は値をヒープの外
// （Impl::eval_bigs）に持ち、結果だけを num_cell が Heap::big で箱にする。述語のモードと誤りは arith.cpp の見出し。
// テスト: tests/pl/bigint.pl、tests/pl/arith.pl、tests/cpp/bigint_limit_test.cpp

#include "arith_internal.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace mosa::arith {

namespace {

BigInt magnitude(const BigInt& value) { return value.negative() ? -value : value; }

// |a| / |b|（どちらも 0 でない）を最近接偶数で double にする。商を 2^54 以上 2^56 未満の整数で求め、余りが 0 でない
// ことを sticky として持てば、丸めの位より下の情報はそれで足りる。非正規化数の範囲では精度を 2^-1074 の位まで縮める。
// どちらの被演算子も double にしないので、double の範囲の外の Big どうしでも商が範囲の中なら値になる。
double quotient_magnitude(const BigInt& a, const BigInt& b) {
  const auto a_bits = static_cast<std::int64_t>(a.bit_length());
  const auto b_bits = static_cast<std::int64_t>(b.bit_length());
  const std::int64_t shift = 55 - (a_bits - b_bits);
  const BigQuotient q = shift >= 0 ? divide_truncate(shift_left(a, static_cast<std::uint64_t>(shift)), b)
                                   : divide_truncate(a, shift_left(b, static_cast<std::uint64_t>(-shift)));
  const auto bits = static_cast<std::uint64_t>(*q.quotient.to_int64());
  const bool sticky = !q.remainder.is_zero();
  const int length = std::bit_width(bits);
  // 値は [2^exponent, 2^(exponent+1)) にある。
  const std::int64_t exponent = length - 1 - shift;
  // 2^1024 以上は無限大。ここで返せば、下の ldexp の指数は int に収まる。
  if (exponent > 1023) return std::numeric_limits<double>::infinity();
  if (exponent < -1075) return 0.0;
  if (exponent == -1075) {
    // 2^-1075 は 0 と最小の非正規化数のちょうど中間。偶数の 0 へ丸め、少しでも大きければ上へ。
    const bool tie = bits == (std::uint64_t{1} << (length - 1)) && !sticky;
    return tie ? 0.0 : std::numeric_limits<double>::denorm_min();
  }
  const std::int64_t precision = std::min<std::int64_t>(53, exponent + 1075);
  const auto drop = static_cast<int>(length - precision);  // 2 以上 55 以下
  const std::uint64_t kept = bits >> drop;
  const std::uint64_t rest = bits & ((std::uint64_t{1} << drop) - 1);
  const std::uint64_t half = std::uint64_t{1} << (drop - 1);
  const bool up = rest > half || (rest == half && (sticky || (kept & 1U) != 0));
  // kept + 1 は 2^53 以下なので double に正確に入り、ldexp は範囲の中なら丸めない（外なら無限大）。
  return std::ldexp(static_cast<double>(kept + (up ? 1U : 0U)), static_cast<int>(drop - shift));
}

// ヒープの確保（Heap::grow_for）と同じく、上限から持った語と使った語を引く。置き場の Big もヒープの外の実行状態
// として引く（ADR 0010 の改訂 2）。
std::size_t room_left(const Impl& machine) {
  const Heap& heap = machine.heap;
  const std::size_t taken = heap.size() + heap.held() + machine.eval_big_words;
  return taken < heap.limit() ? heap.limit() - taken : 0;
}

// Big はその場の値を指し、int64 だけを store に作る。返す参照は次の push_back までに使い終える。
const BigInt& operand(const Impl& machine, Num num, BigInt& store) {
  if (num.big) return big_ref(machine, num);
  store = BigInt(num.ival);
  return store;
}

}  // namespace

Ev from_big(Impl& machine, BigInt value) {
  if (const std::optional<std::int64_t> small = value.to_int64()) return from_int(*small);
  // 葉の写しも途中の値も、置き場に入れる前に上限と比べる。max や単項の - は check_room を通らない。
  const std::size_t words = big_words(value.bit_length());
  if (words > room_left(machine)) throw ResourceLimit("heap_limit");
  machine.eval_bigs.push_back(std::move(value));
  machine.eval_big_words += words;
  Ev ev;
  ev.ok = true;
  ev.num.big = true;
  ev.num.ival = static_cast<std::int64_t>(machine.eval_bigs.size() - 1);
  return ev;
}

bool to_float(Impl& machine, const Goal& goal, Num num, double& out) {
  if (num.flt) {
    out = num.fval;
    return true;
  }
  if (!num.big) {
    out = static_cast<double>(num.ival);
    return true;
  }
  const BigInt& value = big_ref(machine, num);
  if (const std::optional<double> converted = to_double(value)) {
    out = *converted;
    return true;
  }
  // arith.cpp の float_bad と同じく、旗 float_overflow が error でなければ無限大を返す。
  if (machine.float_overflow_error) {
    fail_eval(machine, goal, "float_overflow");
    return false;
  }
  out = value.negative() ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
  return true;
}

int compare_int(const Impl& machine, Num left, Num right) {
  if (!left.big && !right.big) return (left.ival > right.ival) - (left.ival < right.ival);
  BigInt left_store;
  BigInt right_store;
  const std::strong_ordering order =
      operand(machine, left, left_store) <=> operand(machine, right, right_store);
  if (order < 0) return -1;
  return order > 0 ? 1 : 0;
}

// 結果を計算する前に、その大きさの見積もりを残りと比べる。越えるなら計算もしない。
void check_room(const Impl& machine, std::uint64_t bits) {
  if (big_words(bits) > room_left(machine)) throw ResourceLimit("heap_limit");
}

Ev big_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right) {
  BigInt a_store;
  BigInt b_store;
  const BigInt& a = operand(machine, left, a_store);
  const BigInt& b = operand(machine, right, b_store);
  if (name == "+" || name == "-") {
    check_room(machine, std::max(a.bit_length(), b.bit_length()) + 1);
    return from_big(machine, name == "+" ? a + b : a - b);
  }
  if (name == "*") {
    check_room(machine, a.bit_length() + b.bit_length());
    return from_big(machine, a * b);
  }
  // 商と余りは被除数より大きくならないので見積もらない。0 で割る前に止める（divide_* の前提）。
  if (b.is_zero()) {
    fail_eval(machine, goal, "zero_divisor");
    return {};
  }
  if (name == "//" || name == "rem") {
    BigQuotient q = divide_truncate(a, b);
    return from_big(machine, name == "//" ? std::move(q.quotient) : std::move(q.remainder));
  }
  if (name == "div" || name == "mod") {
    BigQuotient q = divide_floor(a, b);
    return from_big(machine, name == "div" ? std::move(q.quotient) : std::move(q.remainder));
  }
  return {};
}

Ev big_divide(Impl& machine, const Goal& goal, Num left, Num right) {
  BigInt a_store;
  BigInt b_store;
  const BigInt& a = operand(machine, left, a_store);
  const BigInt& b = operand(machine, right, b_store);
  if (b.is_zero()) {
    if (machine.float_zero_div_error) {
      fail_eval(machine, goal, "zero_divisor");
      return {};
    }
    // 割られる数は Big なので 0 でない。
    return from_float(a.negative() ? -std::numeric_limits<double>::infinity()
                                   : std::numeric_limits<double>::infinity());
  }
  const bool negative = a.negative() != b.negative();
  // 0 / 負の数は、int64 どうしの / と同じく -0.0 になる。
  const double value = a.is_zero() ? 0.0 : quotient_magnitude(magnitude(a), magnitude(b));
  const double signed_value = negative ? -value : value;
  if (std::isinf(signed_value)) {
    if (machine.float_overflow_error) {
      fail_eval(machine, goal, "float_overflow");
      return {};
    }
  }
  return from_float(signed_value);
}

bool int_odd(const Impl& machine, Num num) {
  if (!num.big) return (num.ival & 1) != 0;
  // Big は 0 でないので、最下位の語がある。
  return (big_ref(machine, num).words().front() & 1U) != 0;
}

Ev big_hat(Impl& machine, Num base, Num exp) {
  // 0、1、-1 の冪は大きくならないので、指数が Big でも見積もらずに値を決める。
  if (!base.big && base.ival >= -1 && base.ival <= 1) {
    if (base.ival == 0) return from_int(!exp.big && exp.ival == 0 ? 1 : 0);
    return from_int(base.ival == -1 && int_odd(machine, exp) ? -1 : 1);
  }
  // 絶対値が 2 以上の底の Big 乗は 2^63 ビットを越え、どのヒープにも入らない。power に渡す前に止める。
  if (exp.big) throw ResourceLimit("heap_limit");
  BigInt store;
  const BigInt& b = operand(machine, base, store);
  // |底| < 2^n なら |底|^e < 2^(e*n)。積が 64 ビットを越えるなら、上限に張り付けて check_room で止める。
  constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
  const std::uint64_t length = b.bit_length();
  const auto e = static_cast<std::uint64_t>(exp.ival);
  check_room(machine, e > kMax / length ? kMax : e * length);
  return from_big(machine, power(b, e));
}

Ev big_shift(Impl& machine, std::string_view name, Num value, Num count) {
  if (name == ">>") {
    // Big のシフトの数は 2^63 以上で、どの値のビット数より大きいので、符号だけが残る。
    if (count.big) return from_int(int_negative(machine, value) ? -1 : 0);
    BigInt store;
    // 右シフトは値より大きくならないので見積もらない。
    return from_big(machine, shift_right(operand(machine, value, store), static_cast<std::uint64_t>(count.ival)));
  }
  // 0 でない値を 2^63 ビット以上ずらした結果は、どのヒープにも入らない。shift_left に渡す前に止める。
  if (count.big) throw ResourceLimit("heap_limit");
  BigInt store;
  const BigInt& a = operand(machine, value, store);
  const auto shift = static_cast<std::uint64_t>(count.ival);
  // shift は 2^63 未満、ビット数はヒープの語数の 64 倍以下なので、和は 64 ビットに収まる。
  check_room(machine, shift + a.bit_length());
  return from_big(machine, shift_left(a, shift));
}

Ev big_gcd(Impl& machine, Num left, Num right) {
  BigInt a_store;
  BigInt b_store;
  // gcd の絶対値は 0 でない方の引数の絶対値を越えないので見積もらない。
  return from_big(machine, gcd(operand(machine, left, a_store), operand(machine, right, b_store)));
}

Ev big_bits(Impl& machine, std::string_view name, Num left, Num right) {
  BigInt a_store;
  BigInt b_store;
  const BigInt& a = operand(machine, left, a_store);
  const BigInt& b = operand(machine, right, b_store);
  // [-2^n, 2^n) の値どうしの /\ \/ xor は同じ範囲に収まるので、結果は n + 1 ビットで足り、+ と同じ見積もりでよい。
  check_room(machine, std::max(a.bit_length(), b.bit_length()) + 1);
  if (name == "/\\") return from_big(machine, bit_and(a, b));
  if (name == "\\/") return from_big(machine, bit_or(a, b));
  return from_big(machine, bit_xor(a, b));
}

Ev from_integral(Impl& machine, double value) {
  // 2^63 は double で正確に表せ、int64 の外の最小の値。-2^63 は int64 に入る。
  if (value >= -0x1p63 && value < 0x1p63) return from_int(static_cast<std::int64_t>(value));
  return from_big(machine, from_integral_double(value));
}

Cell num_cell(Impl& machine, Num num) {
  if (num.flt) return machine.heap.floating(num.fval);
  if (num.big) return machine.heap.big(big_ref(machine, num));
  return machine.heap.integer(num.ival);
}

}  // namespace mosa::arith
