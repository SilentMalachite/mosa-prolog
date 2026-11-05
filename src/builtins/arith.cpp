// 8.6.1 is/2 と 8.7.1 算術比較（=:=、=\=、<、=<、>、>=）が使う式の評価（7.9）。is(?term, @evaluable)、
// =:=(@evaluable, @evaluable) ほか。誤り: instantiation_error, type_error(evaluable, F/A),
// type_error(integer, X), type_error(float, X), type_error(integer_expression, E)（tab/1,2 が使う
// eval_int_expression）, evaluation_error(zero_divisor | undefined | int_overflow | float_overflow),
// resource_error(cyclic_term)（循環する式、iso-notes 7.3.3）。
// テスト: tests/pl/arith.pl, tests/pl/native.pl, tests/stack/deep_builtins.pl, tests/cpp/evaluable_test.cpp

#include "../machine/internal.hpp"
#include "evaluable.hpp"
#include "mosa/runtime.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace mosa {

int float_op(Impl& machine, const Goal& goal, std::string_view name, std::span<const double> in, double& out,
             bool& as_int, std::int64_t& iout);

namespace {

struct Num {
  bool flt = false;
  std::int64_t ival = 0;
  double fval = 0;
};

struct Ev {
  bool ok = false;
  Num num;
};

void fail_inst(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void fail_eval(Impl& machine, const Goal& goal, std::string_view which) {
  raise_formal(machine, formal_evaluation(machine, which), goal.name, goal.arity);
}

void fail_evaluable(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "evaluable", culprit), goal.name, goal.arity);
}

// 循環する式（7.3.3 は未定義）は resource_error(cyclic_term)（iso-notes 7.3.3、レビュー L20 の持ち主の決定 A）。
void fail_cyclic(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_resource(machine, "cyclic_term"), goal.name, goal.arity);
}

void fail_integer(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "integer", culprit), goal.name, goal.arity);
}

double as_float(Num num) { return num.flt ? num.fval : static_cast<double>(num.ival); }

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

Ev number_of(Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  Ev ev;
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) {
    ev.ok = true;
    ev.num.ival = machine.heap.integer_value(cell);
    return ev;
  }
  if (cell.tag() == Tag::Float) {
    ev.ok = true;
    ev.num.flt = true;
    ev.num.fval = machine.heap.float_value(cell);
  }
  return ev;
}

Cell indicator(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  return cell_functor(machine, name, arity);
}

bool both_int(const std::vector<Num>& args) {
  for (const Num& num : args) {
    if (num.flt) return false;
  }
  return true;
}

Ev from_int(std::int64_t value) {
  Ev ev;
  ev.ok = true;
  ev.num.ival = value;
  return ev;
}

Ev from_float(double value) {
  Ev ev;
  ev.ok = true;
  ev.num.flt = true;
  ev.num.fval = value;
  return ev;
}

bool float_bad(Impl& machine, const Goal& goal, double value, bool inputs_finite) {
  if (std::isnan(value)) {
    if (!machine.float_undefined_error) return false;
    fail_eval(machine, goal, "undefined");
    return true;
  }
  if (!std::isfinite(value) && inputs_finite) {
    if (!machine.float_overflow_error) return false;
    fail_eval(machine, goal, "float_overflow");
    return true;
  }
  return false;
}

bool integer_exponent(Num right, std::int64_t& exp) {
  if (!right.flt) {
    exp = right.ival;
    return true;
  }
  if (!std::isfinite(right.fval) || right.fval != std::trunc(right.fval)) return false;
  if (right.fval < static_cast<double>(std::numeric_limits<std::int64_t>::min()) || right.fval >= 0x1p63) {
    return false;
  }
  exp = static_cast<std::int64_t>(right.fval);
  return true;
}

double negative_base_pow(double base, std::int64_t exp) {
  if (exp == 0) return 1.0;
  // 負の底の整数乗は、絶対値のべきに符号を付けてから、負の指数は逆数にする。
  const bool invert = exp < 0;
  const auto mag = invert ? static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(exp)
                          : static_cast<std::uint64_t>(exp);
  const double raised = std::pow(std::fabs(base), static_cast<double>(mag));
  const double signed_value = (mag % 2U) == 0U ? raised : -raised;
  if (!invert) return signed_value;
  if (signed_value == 0) return std::copysign(std::numeric_limits<double>::infinity(), signed_value);
  return 1.0 / signed_value;
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

Ev eval_float_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right) {
  const double a = as_float(left);
  const double b = as_float(right);
  const bool finite = std::isfinite(a) && std::isfinite(b);
  double value = 0;
  if (name == "+") value = a + b;
  else if (name == "-") value = a - b;
  else if (name == "*") value = a * b;
  else if (name == "/") {
    if (b == 0 && machine.float_zero_div_error) {
      fail_eval(machine, goal, "zero_divisor");
      return {};
    }
    value = a / b;
  } else if (name == "**" || name == "^") {
    std::int64_t exp = 0;
    // 9.3.1.3 d の本文どおり、0 の負の冪は undefined（iso-notes 9.3.1.3 d の決定 A）。pow は無限大を返すので先に止める。
    if (a == 0 && b < 0) {
      if (!machine.float_undefined_error) return from_float(std::numeric_limits<double>::quiet_NaN());
      fail_eval(machine, goal, "undefined");
      return {};
    }
    if (a < 0) {
      if (!integer_exponent(right, exp)) {
        if (!machine.float_undefined_error) return from_float(std::numeric_limits<double>::quiet_NaN());
        fail_eval(machine, goal, "undefined");
        return {};
      }
      value = negative_base_pow(a, exp);
    } else {
      value = std::pow(a, b);
    }
  } else {
    return {};
  }
  if (float_bad(machine, goal, value, finite)) return {};
  return from_float(value);
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  auto q = a / b;
  const auto r = a % b;
  if (r != 0 && ((a < 0) != (b < 0))) --q;
  return q;
}

Ev eval_int_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right) {
  if (!need_int(machine, goal, left) || !need_int(machine, goal, right)) return {};
  const auto a = left.ival;
  const auto b = right.ival;
  if ((name == "//" || name == "rem" || name == "div" || name == "mod") && b == 0) {
    fail_eval(machine, goal, "zero_divisor");
    return {};
  }
  // a rem -1 と a mod -1 は 0。C++ の % は最小の整数 % -1 で桁あふれ（未定義動作）になるので先に返す（レビュー L1）。
  if ((name == "rem" || name == "mod") && b == -1) return from_int(0);
  if ((name == "//" || name == "div") && a == std::numeric_limits<std::int64_t>::min() && b == -1) {
    fail_eval(machine, goal, "int_overflow");
    return {};
  }
  std::int64_t out = 0;
  if (name == "+") {
    if (!add_i(a, b, out)) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
  } else if (name == "-") {
    if (!sub_i(a, b, out)) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
  } else if (name == "*") {
    if (!mul_i(a, b, out)) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
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

Ev int_hat(Impl& machine, const Goal& goal, std::int64_t base, std::int64_t exp) {
  if (exp < 0) return negative_hat(machine, goal, base, exp);
  std::int64_t out = 0;
  if (!pow_int(base, exp, out)) {
    fail_eval(machine, goal, "int_overflow");
    return {};
  }
  return from_int(out);
}

// 9.1.6 はこの 6 つを F → I または F → F と定める。整数を黙って float に直さず型エラーにする（iso-notes 9.1.6 の決定 A）。
bool float_only(std::string_view name) {
  return name == "floor" || name == "ceiling" || name == "round" || name == "truncate" ||
         name == "float_integer_part" || name == "float_fractional_part";
}

Ev eval_float_wrap(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  std::array<double, 2> inputs{};
  if (args.size() > inputs.size()) return {};
  if (args.size() == 1 && !args[0].flt && float_only(name)) {
    raise_formal(machine, formal_type(machine, "float", machine.heap.integer(args[0].ival)), goal.name, goal.arity);
    return {};
  }
  // integer/1 の整数はそのまま返す。double を通すと 2^53 を超える値が丸まり、max_integer は int_overflow になる
  // （L2 の敵対的レビュー A2）。
  if (args.size() == 1 && !args[0].flt && name == "integer") return from_int(args[0].ival);
  for (std::size_t index = 0; index < args.size(); ++index) inputs[index] = as_float(args[index]);
  double value = 0;
  bool as_int = false;
  std::int64_t ival = 0;
  const int status = float_op(machine, goal, name, std::span<const double>{inputs.data(), args.size()}, value, as_int, ival);
  if (status == 0) return {};
  if (status < 0) return {};
  if (as_int) return from_int(ival);
  return from_float(value);
}

}  // namespace

namespace {

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

Ev eval_int_rest(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args);

Ev eval_int_fn(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  auto one_int = [&](Num num) { return need_int(machine, goal, num); };
  if (name == "abs" && args.size() == 1) {
    if (args[0].flt) return from_float(std::fabs(args[0].fval));
    std::int64_t out = 0;
    if (!sub_i(0, args[0].ival < 0 ? args[0].ival : 0, out) && args[0].ival < 0) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
    return from_int(args[0].ival < 0 ? out : args[0].ival);
  }
  if (name == "sign" && args.size() == 1) {
    if (args[0].flt) {
      // NaN には符号の値が無いので、そのまま返す（IEEE 754 と SWI-Prolog と同じ。iso-notes 9.1.4 の決定 A）。
      if (std::isnan(args[0].fval)) return from_float(args[0].fval);
      if (args[0].fval == 0) return from_float(std::copysign(0.0, args[0].fval));
      return from_float(std::copysign(1.0, args[0].fval));
    }
    if (args[0].ival < 0) return from_int(-1);
    if (args[0].ival > 0) return from_int(1);
    return from_int(0);
  }
  if ((name == "min" || name == "max") && args.size() == 2) {
    // 型の違う引数は float に直して比べ、float を返す。値の型が値によらず、< と同じ比べ方になる（iso-notes 9.3.8 の
    // 決定 B）。2^53 を超える整数は丸まるので、max(9007199254740993, 9007199254740992.0) は整数より小さい float になる。
    // -0.0 と 0 は値が等しいが別の項なので、-0.0 を小さいとみなし、引数の順によらず max は 0.0、min は -0.0 にする
    // （標準順序の -0.0 @< 0.0 と IEEE 754-2019 の maximum と同じ。iso-notes 9.3.8 の追記、L2 の敵対的レビュー A3）。
    const double left = as_float(args[0]);
    const double right = as_float(args[1]);
    const bool signed_zero = left == 0 && right == 0 && std::signbit(left) && !std::signbit(right);
    const bool left_smaller = both_int(args) ? args[0].ival < args[1].ival : (left < right || signed_zero);
    const bool take_left = name == "min" ? left_smaller : !left_smaller;
    if (both_int(args)) return from_int(take_left ? args[0].ival : args[1].ival);
    return from_float(take_left ? left : right);
  }
  if (name == "gcd" && args.size() == 2) {
    if (!one_int(args[0]) || !one_int(args[1])) return {};
    return gcd_of(machine, goal, args[0].ival, args[1].ival);
  }
  return eval_int_rest(machine, goal, name, args);
}

Ev eval_int_rest(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  auto one_int = [&](Num num) { return need_int(machine, goal, num); };
  if (name == "msb" && args.size() == 1) {
    if (!one_int(args[0])) return {};
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
    if (!one_int(args[0])) return {};
    if (args[0].ival < 0) return from_int(0);
    std::int64_t out = 0;
    if (!add_i(args[0].ival, 1, out)) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
    return from_int(out);
  }
  if ((name == ">>" || name == "<<" || name == "/\\" || name == "\\/" || name == "xor") && args.size() == 2) {
    if (!one_int(args[0]) || !one_int(args[1])) return {};
    if (name == "/\\") return from_int(args[0].ival & args[1].ival);
    if (name == "\\/") return from_int(args[0].ival | args[1].ival);
    if (name == "xor") return from_int(args[0].ival ^ args[1].ival);
    if (name == ">>" || name == "<<") return shift_bits(machine, goal, name, args[0].ival, args[1].ival);
  }
  if (name == "\\" && args.size() == 1) {
    if (!one_int(args[0])) return {};
    return from_int(~args[0].ival);
  }
  return {};
}

Ev eval_known(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  if (name == "+" && args.size() == 1) return args[0].flt ? from_float(args[0].fval) : from_int(args[0].ival);
  if (name == "-" && args.size() == 1) {
    if (args[0].flt) return from_float(args[0].fval == 0 ? std::copysign(0.0, -std::copysign(1.0, args[0].fval))
                                                         : -args[0].fval);
    std::int64_t out = 0;
    if (!sub_i(0, args[0].ival, out)) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
    return from_int(out);
  }
  if ((name == "+" || name == "-" || name == "*") && args.size() == 2) {
    if (both_int(args)) return eval_int_bin(machine, goal, name, args[0], args[1]);
    return eval_float_bin(machine, goal, name, args[0], args[1]);
  }
  if (name == "/" && args.size() == 2) return eval_float_bin(machine, goal, name, args[0], args[1]);
  if ((name == "//" || name == "rem" || name == "div" || name == "mod") && args.size() == 2) {
    return eval_int_bin(machine, goal, name, args[0], args[1]);
  }
  if (name == "^" && args.size() == 2 && !args[0].flt && !args[1].flt) {
    return int_hat(machine, goal, args[0].ival, args[1].ival);
  }
  if ((name == "**" || name == "^") && args.size() == 2) {
    return eval_float_bin(machine, goal, "**", args[0], args[1]);
  }
  if (const Ev extra = eval_int_fn(machine, goal, name, args); extra.ok || machine.exception || machine.eval_abort) {
    return extra;
  }
  if (const Ev floated = eval_float_wrap(machine, goal, name, args);
      floated.ok || machine.exception || machine.eval_abort) {
    return floated;
  }
  return {};
}

Ev constant_of(std::string_view name) {
  if (name == "pi") return from_float(std::acos(-1.0));
  if (name == "e") return from_float(std::exp(1.0));
  if (name == "inf") return from_float(std::numeric_limits<double>::infinity());
  if (name == "nan") return from_float(std::numeric_limits<double>::quiet_NaN());
  if (name == "epsilon") return from_float(std::numeric_limits<double>::epsilon());
  if (name == "max_tagged_integer") return from_int((std::int64_t{1} << 60) - 1);
  return {};
}

// 子を持たない式と、評価できない関数子の誤り。評価できる複合項は nullopt で、呼び手が引数を評価する。
// 7.12.1 は誤りの条件が重なったときの選択を任せる。外側の関数子を引数より先に見る（iso-notes 7.9.1 の決定 B）。
std::optional<Ev> eval_leaf(Impl& machine, const Goal& goal, Cell cell) {
  if (machine.heap.unbound(cell)) {
    fail_inst(machine, goal);
    return Ev{};
  }
  if (const Ev num = number_of(machine, cell); num.ok) return num;
  if (cell.tag() == Tag::Atom) {
    const auto text = machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
    if (const Ev constant = constant_of(text); constant.ok) return constant;
    fail_evaluable(machine, goal, indicator(machine, static_cast<std::uint32_t>(cell.raw() >> 3), 0));
    return Ev{};
  }
  if (cell.tag() != Tag::Str) {
    fail_evaluable(machine, goal, cell);
    return Ev{};
  }
  const auto name = machine.atoms.name(machine.heap.functor_atom(cell));
  if (!is_evaluable(name, machine.heap.functor_arity(cell))) {
    fail_evaluable(machine, goal, indicator(machine, machine.heap.functor_atom(cell), machine.heap.functor_arity(cell)));
    return Ev{};
  }
  return std::nullopt;
}

// 引数を評価し終えた複合項に、その関数子を当てる。
Ev eval_apply(Impl& machine, const Goal& goal, Cell cell, const std::vector<Num>& args) {
  const auto name = machine.atoms.name(machine.heap.functor_atom(cell));
  Ev known = eval_known(machine, goal, name, args);
  if (known.ok || machine.exception || machine.eval_abort) return known;
  fail_evaluable(machine, goal, indicator(machine, machine.heap.functor_atom(cell), machine.heap.functor_arity(cell)));
  return {};
}

// 評価途中の複合項。評価し終えた引数は nums に続けて置き、base は自分の分の始まり。
struct EvalFrame {
  Cell cell;
  std::size_t base;
};

// この深さまでは C++ の再帰で評価する。より深い式は手で積んで評価する（ADR 0011 の改訂 2）。
constexpr int kShallow = 64;

// 循環する式の見張り（Brent の方法を根からの道に当てる）。道の各段は下の段の引数なので、見張りの段は入る段の
// 祖先で、同じ複合項なら式は循環している。終わらない評価は、同じ部分項を毎回同じに評価するので、ある段から先の
// 消えない段は同じ周期で並ぶ。見張りは入った段の数が 1, 2, 4, … に届くたびにその段へ移り、その段が脇道ごと
// 評価を終えれば親へ下がるので、いずれ消えない段に留まり、窓が 1 周期より長くなればその周期のうちに同じ段と
// 出会う。比べるのは入るたびの 1 回だけで、見つけるまでの手間は循環しない部分と 1 周期の評価の数倍に収まる。
// 前は深さが倍になるたびに真ん中の段を探したので、閉路の中に深い脇道があると真ん中が脇道に落ちて見逃し続け、
// 手間が脇道の深さの 2 乗になった（レビュー L20 の敵対的レビュー A1）。C++ の再帰と eval_deep で 1 つを共有する。
// 再帰の側だけ深さで見張りを移すと、循環しない部分が 33〜63 段の閉路を 31 周まで回った（再確認の B1）。
struct CycleWatch {
  Cell watched{};
  std::size_t watched_depth = 0;
  std::size_t entered = 0;
  std::size_t window = 1;

  // 深さ depth（根は 0）の複合項 cell に入る。
  [[nodiscard]] bool enter(Cell cell, std::size_t depth) {
    if (depth == 0) {
      watched = cell;
      return false;
    }
    if (cell.raw() == watched.raw()) return true;
    if (++entered == window) {
      watched = cell;
      watched_depth = depth;
      entered = 0;
      window *= 2;
    }
    return false;
  }

  // 深さ depth の複合項 cell の子が評価を終えた。見張りがその子の側にあれば、この段へ下げる。
  void returned(Cell cell, std::size_t depth) {
    if (watched_depth > depth) {
      watched = cell;
      watched_depth = depth;
    }
  }
};

// 後順に評価する。引数は左から評価するので、最初に投げる誤りは再帰と同じになる。root は深さ depth で、
// eval_at が見張りに入れ終えている。前は循環する式で stack が --heap-max の外で限りなく育った（レビュー L20）。
Ev eval_deep(Impl& machine, const Goal& goal, Cell root, std::size_t depth, CycleWatch& watch) {
  std::vector<EvalFrame> stack{{root, 0}};
  std::vector<Num> nums;
  while (true) {
    const EvalFrame top = stack.back();
    const std::size_t done = nums.size() - top.base;
    if (done < machine.heap.functor_arity(top.cell)) {
      const Cell next = machine.heap.deref(machine.heap.arg(top.cell, static_cast<std::uint16_t>(done)));
      if (const auto leaf = eval_leaf(machine, goal, next)) {
        if (!leaf->ok) return {};
        nums.push_back(leaf->num);
      } else {
        stack.push_back(EvalFrame{next, nums.size()});
        if (watch.enter(next, depth + stack.size() - 1)) {
          fail_cyclic(machine, goal);
          return {};
        }
      }
      continue;
    }
    const std::vector<Num> args(nums.begin() + static_cast<std::ptrdiff_t>(top.base), nums.end());
    const Ev ev = eval_apply(machine, goal, top.cell, args);
    if (!ev.ok) return ev;
    nums.resize(top.base);
    stack.pop_back();
    if (stack.empty()) return ev;
    watch.returned(stack.back().cell, depth + stack.size() - 1);
    nums.push_back(ev.num);
  }
}

Ev eval_at(Impl& machine, const Goal& goal, Cell cell, int depth, CycleWatch& watch) {
  cell = machine.heap.deref(cell);
  if (auto leaf = eval_leaf(machine, goal, cell)) return *leaf;
  const auto level = static_cast<std::size_t>(depth);
  if (watch.enter(cell, level)) {
    fail_cyclic(machine, goal);
    return {};
  }
  if (depth >= kShallow) return eval_deep(machine, goal, cell, level, watch);
  const auto arity = machine.heap.functor_arity(cell);
  std::vector<Num> args;
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) {
    const Ev ev = eval_at(machine, goal, machine.heap.arg(cell, index), depth + 1, watch);
    if (!ev.ok) return {};
    watch.returned(cell, level);
    args.push_back(ev.num);
  }
  return eval_apply(machine, goal, cell, args);
}

Ev eval_expr(Impl& machine, const Goal& goal, Cell cell) {
  CycleWatch watch;
  return eval_at(machine, goal, cell, 0, watch);
}

Cell num_cell(Impl& machine, Num num) {
  if (num.flt) return machine.heap.floating(num.fval);
  return machine.heap.integer(num.ival);
}

bool cmp_nums(Num left, Num right, int mode) {
  int order = 0;
  if (left.flt || right.flt) {
    const double a = as_float(left);
    const double b = as_float(right);
    // NaN との比較は大小も等号も成り立たない。=\= は =:= の否定なので、ここが失敗すれば成立する。
    if (std::isnan(a) || std::isnan(b)) return false;
    if (a < b) order = -1;
    else if (a > b) order = 1;
  } else if (left.ival < right.ival) order = -1;
  else if (left.ival > right.ival) order = 1;
  if (mode == -2) return order < 0;
  if (mode == -1) return order <= 0;
  if (mode == 0) return order == 0;
  if (mode == 1) return order >= 0;
  return order > 0;
}

bool run_cmp(Impl& machine, const Goal& goal, int mode) {
  const Ev left = eval_expr(machine, goal, goal.arg(machine, 0));
  if (!left.ok) return true;
  const Ev right = eval_expr(machine, goal, goal.arg(machine, 1));
  if (!right.ok) return true;
  if (cmp_nums(left.num, right.num, mode)) machine.ip = goal.cont;
  else machine.failed = true;
  return true;
}

}  // namespace

bool run_arith(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "is" && goal.arity == 2) {
    const Ev value = eval_expr(machine, goal, goal.arg(machine, 1));
    if (!value.ok) return true;
    if (!do_unify(machine, goal.arg(machine, 0), num_cell(machine, value.num))) machine.failed = true;
    else machine.ip = goal.cont;
    return true;
  }
  if (goal.arity != 2) return false;
  if (name == "=:=") return run_cmp(machine, goal, 0);
  if (name == "=\\=") {
    if (!run_cmp(machine, goal, 0) || machine.exception || machine.eval_abort) return true;
    if (machine.failed) {
      machine.failed = false;
      machine.ip = goal.cont;
    } else {
      machine.failed = true;
    }
    return true;
  }
  if (name == "<") return run_cmp(machine, goal, -2);
  if (name == "=<") return run_cmp(machine, goal, -1);
  if (name == ">") return run_cmp(machine, goal, 2);
  if (name == ">=") return run_cmp(machine, goal, 1);
  return false;
}

std::optional<std::int64_t> eval_int_expression(Impl& machine, const Goal& goal, Cell cell) {
  const Ev value = eval_expr(machine, goal, cell);
  if (!value.ok) return std::nullopt;
  if (value.num.flt) {
    raise_formal(machine, formal_type(machine, "integer_expression", cell), goal.name, goal.arity);
    return std::nullopt;
  }
  return value.num.ival;
}

namespace rt {

namespace {

// 生成コードから直に呼ぶときの誤りの文脈（context(is/2, _) など）。run_arith は枠から名前を読むが、ここには枠が無いので、
// 生成コードが単位の表から渡す述語のアトムを置く。
Goal arith_goal(std::uint32_t name) {
  Goal goal;
  goal.name = name;
  goal.arity = 2;
  return goal;
}

}  // namespace

bool arith_is(Machine::Impl& machine, std::uint32_t name, Cell left, Cell right) {
  const Ev value = eval_expr(machine, arith_goal(name), right);
  if (!value.ok) return false;
  if (do_unify(machine, left, num_cell(machine, value.num))) return true;
  machine.failed = true;
  return false;
}

bool arith_compare(Machine::Impl& machine, Compare op, std::uint32_t name, Cell left, Cell right) {
  // Compare の順（Eq、Ne、Lt、Le、Gt、Ge）に run_cmp の mode を並べる。
  static constexpr std::array<int, 6> kModes{0, 0, -2, -1, 2, 1};
  const Goal goal = arith_goal(name);
  const Ev lhs = eval_expr(machine, goal, left);
  if (!lhs.ok) return false;
  const Ev rhs = eval_expr(machine, goal, right);
  if (!rhs.ok) return false;
  // =\= は =:= の否定（run_arith）。NaN はどちらとも比べられないので =\= だけが成り立つ（cmp_nums）。
  const bool holds = cmp_nums(lhs.num, rhs.num, kModes[static_cast<std::size_t>(op)]) != (op == Compare::Ne);
  if (!holds) machine.failed = true;
  return holds;
}

}  // namespace rt

}
