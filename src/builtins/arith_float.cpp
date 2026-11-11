// arith.cpp の式の評価（7.9）が使う浮動小数の評価可能関数の分担。述語のモードと誤りは arith.cpp の見出し。
// テスト: tests/pl/arith.pl、tests/cpp/evaluable_test.cpp

#include "../machine/internal.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace mosa {

namespace {

void fail_eval(Impl& machine, const Goal& goal, std::string_view which) {
  raise_formal(machine, formal_evaluation(machine, which), goal.name, goal.arity);
}

bool bad_float(Impl& machine, const Goal& goal, double value, bool finite_in) {
  if (std::isnan(value)) {
    if (!machine.float_undefined_error) return false;
    fail_eval(machine, goal, "undefined");
    return true;
  }
  if (!std::isfinite(value) && finite_in) {
    if (!machine.float_overflow_error) return false;
    fail_eval(machine, goal, "float_overflow");
    return true;
  }
  return false;
}

bool to_int(Impl& machine, const Goal& goal, double value, std::int64_t& out) {
  if (!std::isfinite(value)) {
    fail_eval(machine, goal, std::isnan(value) ? "undefined" : "int_overflow");
    return false;
  }
  const auto lo = static_cast<double>(std::numeric_limits<std::int64_t>::min());
  // 2^63-1 を double にすると 2^63 に丸まる。2^63 以上は int64 に入らない。
  if (value < lo || value >= 0x1p63) {
    fail_eval(machine, goal, "int_overflow");
    return false;
  }
  out = static_cast<std::int64_t>(value);
  return true;
}

// 9.1.6 の round(x) = floor(x + 1/2)。x + 0.5 を浮動小数点で足すと 0.49999999999999994 や
// 2^52 を超える奇数で丸めが起きるので、端数を引き算（誤差が出ない）で取り出して比べる。
double round_half_up(double value) {
  const double base = std::floor(value);
  return value - base >= 0.5 ? base + 1.0 : base;
}

int float_round(Impl& machine, const Goal& goal, std::string_view name, double value, bool& as_int,
                std::int64_t& iout) {
  auto integral = [&](double rounded) {
    if (!to_int(machine, goal, rounded, iout)) return -1;
    as_int = true;
    return 1;
  };
  // integer/1 は SWI-Prolog の拡張で、近い整数へ丸め、ちょうど半分は 0 から遠い方へ（std::round）。round/1 は
  // 9.1.6.1 の floor(x + 1/2) なので、負の半分だけが違う（iso-notes 9.1.4 の決定 A、レビュー L2）。
  if (name == "integer") return integral(std::round(value));
  if (name == "truncate") return integral(std::trunc(value));
  if (name == "floor") return integral(std::floor(value));
  if (name == "ceiling") return integral(std::ceil(value));
  if (name == "round") return integral(round_half_up(value));
  return 0;
}

// 定義域の外。undefined の旗が off なら、誤りにせず NaN を返す。
int undefined_value(Impl& machine, const Goal& goal, double& out) {
  if (!machine.float_undefined_error) {
    out = std::numeric_limits<double>::quiet_NaN();
    return 1;
  }
  fail_eval(machine, goal, "undefined");
  return -1;
}

int checked(Impl& machine, const Goal& goal, double value, bool finite_in, double& out) {
  if (bad_float(machine, goal, value, finite_in)) return -1;
  out = value;
  return 1;
}

int float_unary(Impl& machine, const Goal& goal, std::string_view name, double value, double& out) {
  auto apply = [&](double (*fn)(double)) { return checked(machine, goal, fn(value), std::isfinite(value), out); };
  if (name == "sqrt") return value < 0 ? undefined_value(machine, goal, out) : apply(std::sqrt);
  if (name == "sin") return apply(std::sin);
  if (name == "cos") return apply(std::cos);
  if (name == "tan") return apply(std::tan);
  if (name == "asin") return apply(std::asin);
  if (name == "acos") return apply(std::acos);
  if (name == "atan") return apply(std::atan);
  if (name == "exp") return apply(std::exp);
  return 0;
}

// log(B, X) の底 1 は ln B = 0 で割ることになる。底として意味が無いので、0 以下と同じく undefined。
int float_log(Impl& machine, const Goal& goal, std::string_view name, std::span<const double> in, double& out) {
  const bool with_base = name == "log" && in.size() == 2;
  if (!with_base && in.size() != 1) return 0;
  if (in[0] <= 0 || (with_base && (in[0] == 1 || in[1] <= 0))) return undefined_value(machine, goal, out);
  if (with_base) {
    return checked(machine, goal, std::log(in[1]) / std::log(in[0]), std::isfinite(in[0]) && std::isfinite(in[1]),
                   out);
  }
  return checked(machine, goal, name == "log" ? std::log(in[0]) : std::log2(in[0]), std::isfinite(in[0]), out);
}

int float_atan2(Impl& machine, const Goal& goal, std::span<const double> in, double& out) {
  if (in.size() != 2) return 0;
  if (in[0] == 0 && in[1] == 0) return undefined_value(machine, goal, out);
  return checked(machine, goal, std::atan2(in[0], in[1]), true, out);
}

}  // namespace

int float_op(Impl& machine, const Goal& goal, std::string_view name, std::span<const double> in, double& out,
             bool& as_int, std::int64_t& iout) {
  const std::size_t count = in.size();
  if (count == 1) {
    if (const int done = float_unary(machine, goal, name, in[0], out); done != 0) return done;
  }
  if (name == "log" || name == "log2") return float_log(machine, goal, name, in, out);
  if (name == "atan2") return float_atan2(machine, goal, in, out);
  if (name == "copysign" && count == 2) {
    out = std::copysign(in[0], in[1]);
    return 1;
  }
  if (name == "nexttoward" && count == 2) {
    return checked(machine, goal, std::nexttoward(in[0], in[1]), std::isfinite(in[0]), out);
  }
  if (count != 1) return 0;
  if (name == "float") {
    out = in[0];
    return 1;
  }
  if (name == "float_integer_part") {
    out = std::trunc(in[0]);
    return 1;
  }
  if (name == "float_fractional_part") {
    out = in[0] - std::trunc(in[0]);
    return 1;
  }
  if (name == "integer" || name == "truncate" || name == "floor" || name == "ceiling" || name == "round") {
    return float_round(machine, goal, name, in[0], as_int, iout);
  }
  return 0;
}

}
