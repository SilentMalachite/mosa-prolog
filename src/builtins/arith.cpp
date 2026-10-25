#include "../machine/internal.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace mosa {

int float_op(Impl& machine, const Goal& goal, std::string_view name, const double* in, int count, double& out,
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

bool need_int(Impl& machine, const Goal& goal, Cell cell, Num num) {
  if (!num.flt) return true;
  fail_integer(machine, goal, cell);
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
    fail_eval(machine, goal, "undefined");
    return true;
  }
  if (!std::isfinite(value) && inputs_finite) {
    fail_eval(machine, goal, "float_overflow");
    return true;
  }
  return false;
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
    if (b == 0) {
      fail_eval(machine, goal, "zero_divisor");
      return {};
    }
    value = a / b;
  } else if (name == "**" || name == "^") {
    if (a < 0 && !right.flt && right.ival < 0) {
      fail_eval(machine, goal, "undefined");
      return {};
    }
    value = std::pow(a, b);
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

Ev eval_int_bin(Impl& machine, const Goal& goal, std::string_view name, Num left, Num right, Cell raw) {
  if (!need_int(machine, goal, raw, left) || !need_int(machine, goal, raw, right)) return {};
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
  else if (name == "mod") out = a - b * floor_div(a, b);
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

Ev eval_power(Impl& machine, const Goal& goal, Num left, Num right) {
  if (!left.flt && !right.flt && right.ival >= 0) {
    std::int64_t out = 0;
    if (!pow_int(left.ival, right.ival, out)) {
      fail_eval(machine, goal, "int_overflow");
      return {};
    }
    return from_int(out);
  }
  return eval_float_bin(machine, goal, "**", left, right);
}

Ev eval_float_wrap(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args) {
  double inputs[2] = {};
  if (args.size() > 2) return {};
  for (std::size_t index = 0; index < args.size(); ++index) inputs[index] = as_float(args[index]);
  double value = 0;
  bool as_int = false;
  std::int64_t ival = 0;
  const int status = float_op(machine, goal, name, inputs, static_cast<int>(args.size()), value, as_int, ival);
  if (status == 0) return {};
  if (status < 0) return {};
  if (as_int) return from_int(ival);
  return from_float(value);
}

}  // namespace

namespace {

Ev eval_int_rest(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args, Cell raw);

Ev eval_int_fn(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args, Cell raw) {
  auto one_int = [&](Num num) {
    return need_int(machine, goal, raw, num);
  };
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
      if (args[0].fval == 0) return from_int(0);
      return from_int(args[0].fval < 0 ? -1 : 1);
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
  return eval_int_rest(machine, goal, name, args, raw);
}

Ev eval_int_rest(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args, Cell raw) {
  auto one_int = [&](Num num) { return need_int(machine, goal, raw, num); };
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
    if (args[1].ival < 0 || args[1].ival >= 63) {
      fail_eval(machine, goal, "undefined");
      return {};
    }
    const auto shift = static_cast<unsigned>(args[1].ival);
    if (name == "<<") {
      if (args[0].ival < 0 || (shift > 0 && args[0].ival > (std::numeric_limits<std::int64_t>::max() >> shift))) {
        fail_eval(machine, goal, "int_overflow");
        return {};
      }
      return from_int(args[0].ival << shift);
    }
    return from_int(args[0].ival >> shift);
  }
  if (name == "\\" && args.size() == 1) {
    if (!one_int(args[0])) return {};
    return from_int(~args[0].ival);
  }
  return {};
}

Ev eval_known(Impl& machine, const Goal& goal, std::string_view name, const std::vector<Num>& args, Cell raw) {
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
    if (both_int(args)) return eval_int_bin(machine, goal, name, args[0], args[1], raw);
    return eval_float_bin(machine, goal, name, args[0], args[1]);
  }
  if (name == "/" && args.size() == 2) return eval_float_bin(machine, goal, name, args[0], args[1]);
  if ((name == "//" || name == "rem" || name == "div" || name == "mod") && args.size() == 2) {
    return eval_int_bin(machine, goal, name, args[0], args[1], raw);
  }
  if ((name == "**" || name == "^") && args.size() == 2) return eval_power(machine, goal, args[0], args[1]);
  if (const Ev extra = eval_int_fn(machine, goal, name, args, raw); extra.ok || machine.exception) return extra;
  if (const Ev floated = eval_float_wrap(machine, goal, name, args); floated.ok || machine.exception) return floated;
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
  std::vector<Num> args;
  if (!eval_args(machine, goal, cell, args)) return {};
  Ev known = eval_known(machine, goal, name, args, cell);
  if (known.ok || machine.exception) return known;
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
    if (!run_cmp(machine, goal, 0) || machine.exception) return true;
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

}
