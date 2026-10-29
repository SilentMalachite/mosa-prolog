#include "../machine/internal.hpp"
#include "evaluable.hpp"
#include "mosa/runtime.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
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

Ev eval_expr(Impl& machine, const Goal& goal, Cell cell);

bool eval_args(Impl& machine, const Goal& goal, Cell cell, std::vector<Num>& args) {
  const auto arity = machine.heap.functor_arity(cell);
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) {
    const Ev ev = eval_expr(machine, goal, machine.heap.arg(cell, index));
    if (!ev.ok) return false;
    args.push_back(ev.num);
  }
  return true;
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
  if ((name == "//" || name == "rem" || name == "div" || name == "mod") &&
      a == std::numeric_limits<std::int64_t>::min() && b == -1) {
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
      if (args[0].fval == 0) return from_float(std::copysign(0.0, args[0].fval));
      return from_float(std::copysign(1.0, args[0].fval));
    }
    if (args[0].ival < 0) return from_int(-1);
    if (args[0].ival > 0) return from_int(1);
    return from_int(0);
  }
  if ((name == "min" || name == "max") && args.size() == 2) {
    const bool left_smaller = both_int(args) ? args[0].ival < args[1].ival : as_float(args[0]) < as_float(args[1]);
    const bool take_left = name == "min" ? left_smaller : !left_smaller;
    if (both_int(args)) return from_int(take_left ? args[0].ival : args[1].ival);
    return from_float(take_left ? as_float(args[0]) : as_float(args[1]));
  }
  if (name == "gcd" && args.size() == 2) {
    if (!one_int(args[0]) || !one_int(args[1])) return {};
    auto a = args[0].ival < 0 ? -args[0].ival : args[0].ival;
    auto b = args[1].ival < 0 ? -args[1].ival : args[1].ival;
    if (args[0].ival == std::numeric_limits<std::int64_t>::min() ||
        args[1].ival == std::numeric_limits<std::int64_t>::min()) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
    while (b != 0) {
      const auto next = a % b;
      a = b;
      b = next;
    }
    return from_int(a);
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

Ev eval_expr(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) {
    fail_inst(machine, goal);
    return {};
  }
  if (const Ev num = number_of(machine, cell); num.ok) return num;
  if (cell.tag() == Tag::Atom) {
    const auto text = machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
    if (const Ev constant = constant_of(text); constant.ok) return constant;
    fail_evaluable(machine, goal, indicator(machine, static_cast<std::uint32_t>(cell.raw() >> 3), 0));
    return {};
  }
  if (cell.tag() != Tag::Str) {
    fail_evaluable(machine, goal, cell);
    return {};
  }
  const auto name = machine.atoms.name(machine.heap.functor_atom(cell));
  // 7.12.1 は誤りの条件が重なったときの選択を任せる。外側の関数子を先に見る（iso-notes 7.9.1 の決定 B）。
  if (!is_evaluable(name, machine.heap.functor_arity(cell))) {
    fail_evaluable(machine, goal, indicator(machine, machine.heap.functor_atom(cell), machine.heap.functor_arity(cell)));
    return {};
  }
  std::vector<Num> args;
  if (!eval_args(machine, goal, cell, args)) return {};
  Ev known = eval_known(machine, goal, name, args);
  if (known.ok || machine.exception || machine.eval_abort) return known;
  fail_evaluable(machine, goal, indicator(machine, machine.heap.functor_atom(cell), machine.heap.functor_arity(cell)));
  return {};
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
