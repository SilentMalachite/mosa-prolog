// 7.9 の式の評価のうち整数の演算（9.1.3 の + - * と // rem div mod、9.3 の abs sign min max ほか、9.4 のビット演算
// とシフト）。int64 の道を先に試し、あふれたときと Big の引数のときだけ arith_big.cpp の BigInt に回す。
// 述語のモードと誤りは arith.cpp の見出し。テスト: tests/pl/arith.pl、tests/pl/bigint.pl、tests/cpp/evaluable_test.cpp

#include "arith_internal.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace mosa::arith {

namespace {

bool add_i(std::int64_t a, std::int64_t b, std::int64_t& out) {
  if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
      (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b)) {
    return false;
  }
  out = a + b;
  return true;
}

bool sub_i(std::int64_t a, std::int64_t b, std::int64_t& out) {
  if ((b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) ||
      (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b)) {
    return false;
  }
  out = a - b;
  return true;
}

bool mul_i(std::int64_t a, std::int64_t b, std::int64_t& out) {
  if (a == 0 || b == 0) {
    out = 0;
    return true;
  }
  if ((a == -1 && b == std::numeric_limits<std::int64_t>::min()) ||
      (b == -1 && a == std::numeric_limits<std::int64_t>::min())) {
    return false;
  }
  if (a > 0 && b > 0 && a > std::numeric_limits<std::int64_t>::max() / b) return false;
  if (a > 0 && b < 0 && b < std::numeric_limits<std::int64_t>::min() / a) return false;
  if (a < 0 && b > 0 && a < std::numeric_limits<std::int64_t>::min() / b) return false;
  if (a < 0 && b < 0 && a < std::numeric_limits<std::int64_t>::max() / b) return false;
  out = a * b;
  return true;
}

bool need_int(Impl& machine, const Goal& goal, Num num) {
  if (!num.flt) return true;
  fail_integer(machine, goal, machine.heap.floating(num.fval));
  return false;
}

Ev shift_bits(Impl& machine, const Goal& goal, std::string_view name, std::int64_t value, std::int64_t count) {
  if (count < 0) {
    fail_eval(machine, goal, "undefined");
    return {};
  }
  if (name == ">>") {
    if (count >= 64) return from_int(value < 0 ? std::int64_t{-1} : 0);
    return from_int(value >> static_cast<unsigned>(count));
  }
  if (count >= 64) {
    if (value == 0) return from_int(0);
    fail_eval(machine, goal, "int_overflow");
    return {};
  }
  const auto shift = static_cast<unsigned>(count);
  if (shift != 0) {
    // 結果が int64 に収まらない左シフトだけを落とす。負数は二の補数のまま残す。
    const bool overflow = value >= 0 ? value > (std::numeric_limits<std::int64_t>::max() >> shift)
                                     : value < (std::numeric_limits<std::int64_t>::min() >> shift);
    if (overflow) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
  }
  const auto bits = static_cast<std::uint64_t>(value) << shift;
  return from_int(static_cast<std::int64_t>(bits));
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  auto q = a / b;
  const auto r = a % b;
  if (r != 0 && ((a < 0) != (b < 0))) --q;
  return q;
}

bool pow_int(std::int64_t base, std::int64_t exp, std::int64_t& out) {
  if (exp < 0) return false;
  std::int64_t acc = 1;
  std::int64_t factor = base;
  auto rest = exp;
  while (rest > 0) {
    if ((rest & 1) != 0 && !mul_i(acc, factor, acc)) return false;
    rest >>= 1;
    if (rest > 0 && !mul_i(factor, factor, factor)) return false;
  }
  out = acc;
  return true;
}

Ev negative_hat(Impl& machine, const Goal& goal, std::int64_t base, std::int64_t exp) {
  if (base == 0) {
    fail_eval(machine, goal, "undefined");
    return {};
  }
  if (base == 1) return from_int(1);
  // -1 の負の冪は整数のまま。奇数なら -1、偶数なら 1。
  if (base == -1) return from_int(exp % 2 == 0 ? 1 : -1);
  // 結果が整数にならないので float の型エラー（iso-notes 9.3.10.3 の決定 B）。2^(-1) も同じ。
  raise_formal(machine, formal_type(machine, "float", machine.heap.integer(base)), goal.name, goal.arity);
  return {};
}

// 絶対値は符号なしで取る。-(-2^63) は int64 に収まらず未定義動作になる（レビュー L1）。値が 2^63 になる
// gcd(最小の整数, 0) と gcd(最小の整数, 最小の整数) だけがあふれる。
Ev gcd_of(Impl& machine, const Goal& goal, std::int64_t left, std::int64_t right) {
  const auto magnitude = [](std::int64_t value) {
    const auto bits = static_cast<std::uint64_t>(value);
    return value < 0 ? std::uint64_t{0} - bits : bits;
  };
  std::uint64_t a = magnitude(left);
  std::uint64_t b = magnitude(right);
  while (b != 0) {
    const auto next = a % b;
    a = b;
    b = next;
  }
  if (a > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    fail_eval(machine, goal, "int_overflow");
    return {};
  }
  return from_int(static_cast<std::int64_t>(a));
}

Ev eval_int_rest(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  auto one_int = [&](Num num) { return need_int(machine, goal, num); };
  if (name == "msb" && args.size() == 1) {
    if (!one_int(args[0])) return {};
    if (args[0].big) {
      const BigInt& value = big_ref(machine, args[0]);
      if (value.negative()) {
        fail_eval(machine, goal, "undefined");
        return {};
      }
      return from_int(static_cast<std::int64_t>(value.bit_length()) - 1);
    }
    if (args[0].ival <= 0) {
      fail_eval(machine, goal, "undefined");
      return {};
    }
    int bits = 0;
    auto value = static_cast<std::uint64_t>(args[0].ival);
    while (value > 1) {
      value >>= 1;
      ++bits;
    }
    return from_int(bits);
  }
  if (name == "succ_or_zero" && args.size() == 1) {
    if (big_pending(machine, goal, args) || !one_int(args[0])) return {};
    if (args[0].ival < 0) return from_int(0);
    std::int64_t out = 0;
    if (!add_i(args[0].ival, 1, out)) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
    return from_int(out);
  }
  if ((name == ">>" || name == "<<" || name == "/\\" || name == "\\/" || name == "xor") && args.size() == 2) {
    if (big_pending(machine, goal, args) || !one_int(args[0]) || !one_int(args[1])) return {};
    if (name == "/\\") return from_int(args[0].ival & args[1].ival);
    if (name == "\\/") return from_int(args[0].ival | args[1].ival);
    if (name == "xor") return from_int(args[0].ival ^ args[1].ival);
    if (name == ">>" || name == "<<") return shift_bits(machine, goal, name, args[0].ival, args[1].ival);
  }
  if (name == "\\" && args.size() == 1) {
    if (big_pending(machine, goal, args) || !one_int(args[0])) return {};
    return from_int(~args[0].ival);
  }
  return {};
}

}  // namespace

bool big_pending(Impl& machine, const Goal& goal, const std::vector<Num>& args) {
  for (const Num& num : args) {
    if (num.big) {
      fail_eval(machine, goal, "int_overflow");
      return true;
    }
  }
  return false;
}

Ev negate_int(Impl& machine, Num num) {
  if (num.big) return from_big(machine, -big_ref(machine, num));
  std::int64_t out = 0;
  // -(-2^63) だけが int64 に入らず、2^63 の Big になる。
  if (!sub_i(0, num.ival, out)) return from_big(machine, -BigInt(num.ival));
  return from_int(out);
}

Ev eval_int_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right) {
  if (!need_int(machine, goal, left) || !need_int(machine, goal, right)) return {};
  if (left.big || right.big) return big_bin(machine, goal, name, left, right);
  const auto a = left.ival;
  const auto b = right.ival;
  if ((name == "//" || name == "rem" || name == "div" || name == "mod") && b == 0) {
    fail_eval(machine, goal, "zero_divisor");
    return {};
  }
  // a rem -1 と a mod -1 は 0。C++ の % は最小の整数 % -1 で桁あふれ（未定義動作）になるので先に返す（レビュー L1）。
  if ((name == "rem" || name == "mod") && b == -1) return from_int(0);
  // -2^63 を -1 で割った商だけが int64 に入らず、2^63 の Big になる。
  if ((name == "//" || name == "div") && a == std::numeric_limits<std::int64_t>::min() && b == -1) {
    return from_big(machine, -BigInt(a));
  }
  std::int64_t out = 0;
  // int64 であふれたときだけ BigInt で計算し直す。結果は 128 ビット以下なので大きさを見積もらない。
  if (name == "+") {
    if (!add_i(a, b, out)) return from_big(machine, BigInt(a) + BigInt(b));
  } else if (name == "-") {
    if (!sub_i(a, b, out)) return from_big(machine, BigInt(a) - BigInt(b));
  } else if (name == "*") {
    if (!mul_i(a, b, out)) return from_big(machine, BigInt(a) * BigInt(b));
  } else if (name == "//") out = a / b;
  else if (name == "rem") out = a % b;
  else if (name == "div") out = floor_div(a, b);
  else if (name == "mod") {
    // b * q は int64 に収まらないことがある。剰余の符号を直すと結果は必ず収まる。
    auto rem = a % b;
    if (rem != 0 && ((a < 0) != (b < 0))) rem += b;
    out = rem;
  }
  else return {};
  return from_int(out);
}

Ev int_hat(Impl& machine, const Goal& goal, std::int64_t base, std::int64_t exp) {
  if (exp < 0) return negative_hat(machine, goal, base, exp);
  std::int64_t out = 0;
  if (!pow_int(base, exp, out)) {
    fail_eval(machine, goal, "int_overflow");
    return {};
  }
  return from_int(out);
}

Ev eval_int_fn(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  auto one_int = [&](Num num) { return need_int(machine, goal, num); };
  if (name == "abs" && args.size() == 1) {
    if (args[0].flt) return from_float(std::fabs(args[0].fval));
    const bool negative = args[0].big ? big_ref(machine, args[0]).negative() : args[0].ival < 0;
    return negative ? negate_int(machine, args[0]) : from_num(args[0]);
  }
  if (name == "sign" && args.size() == 1) {
    if (args[0].flt) {
      // NaN には符号の値が無いので、そのまま返す（IEEE 754 と SWI-Prolog と同じ。iso-notes 9.1.4 の決定 A）。
      if (std::isnan(args[0].fval)) return from_float(args[0].fval);
      if (args[0].fval == 0) return from_float(std::copysign(0.0, args[0].fval));
      return from_float(std::copysign(1.0, args[0].fval));
    }
    if (args[0].big) return from_int(big_ref(machine, args[0]).negative() ? -1 : 1);
    if (args[0].ival < 0) return from_int(-1);
    if (args[0].ival > 0) return from_int(1);
    return from_int(0);
  }
  if ((name == "min" || name == "max") && args.size() == 2) {
    // 型の違う引数は float に直して比べ、float を返す。値の型が値によらず、< と同じ比べ方になる（iso-notes 9.3.8 の
    // 決定 B）。2^53 を超える整数は丸まるので、max(9007199254740993, 9007199254740992.0) は整数より小さい float になる。
    // -0.0 と 0 は値が等しいが別の項なので、-0.0 を小さいとみなし、引数の順によらず max は 0.0、min は -0.0 にする
    // （標準順序の -0.0 @< 0.0 と IEEE 754-2019 の maximum と同じ。iso-notes 9.3.8 の追記、L2 の敵対的レビュー A3）。
    // 整数どうしは Big を含めて正確に比べ、選んだ方をそのまま返す。
    if (both_int(args)) {
      const bool left_smaller = compare_int(machine, args[0], args[1]) < 0;
      const bool take_left = name == "min" ? left_smaller : !left_smaller;
      return from_num(take_left ? args[0] : args[1]);
    }
    double left = 0;
    double right = 0;
    if (!to_float(machine, goal, args[0], left) || !to_float(machine, goal, args[1], right)) return {};
    const bool signed_zero = left == 0 && right == 0 && std::signbit(left) && !std::signbit(right);
    const bool left_smaller = left < right || signed_zero;
    const bool take_left = name == "min" ? left_smaller : !left_smaller;
    return from_float(take_left ? left : right);
  }
  if (name == "gcd" && args.size() == 2) {
    if (big_pending(machine, goal, args) || !one_int(args[0]) || !one_int(args[1])) return {};
    return gcd_of(machine, goal, args[0].ival, args[1].ival);
  }
  return eval_int_rest(machine, goal, name, args);
}

}  // namespace mosa::arith
