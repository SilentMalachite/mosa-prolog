#include "../machine/internal.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>

namespace mosa {

namespace {

void fail_eval(Impl& machine, const Goal& goal, std::string_view which) {
  raise_formal(machine, formal_evaluation(machine, which), goal.name, goal.arity);
}

bool bad_float(Impl& machine, const Goal& goal, double value, bool finite_in) {
  if (std::isnan(value)) {
    fail_eval(machine, goal, "undefined");
    return true;
  }
  if (!std::isfinite(value) && finite_in) {
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
  const auto hi = static_cast<double>(std::numeric_limits<std::int64_t>::max());
  if (value < lo || value > hi) {
    fail_eval(machine, goal, "int_overflow");
    return false;
  }
  out = static_cast<std::int64_t>(value);
  return true;
}

}  // namespace

int float_op(Impl& machine, const Goal& goal, std::string_view name, const double* in, int count, double& out,
             bool& as_int, std::int64_t& iout) {
  auto unary = [&](double (*fn)(double)) {
    if (count != 1) return 0;
    const double value = fn(in[0]);
    if (bad_float(machine, goal, value, std::isfinite(in[0]))) return -1;
    out = value;
    return 1;
  };
  if (name == "sqrt") {
    if (count != 1) return 0;
    if (in[0] < 0) {
      fail_eval(machine, goal, "undefined");
      return -1;
    }
    return unary(std::sqrt);
  }
  if (name == "sin") return unary(std::sin);
  if (name == "cos") return unary(std::cos);
  if (name == "tan") return unary(std::tan);
  if (name == "asin") return unary(std::asin);
  if (name == "acos") return unary(std::acos);
  if (name == "atan") return unary(std::atan);
  if (name == "exp") return unary(std::exp);
  if (name == "log" || name == "log2") {
    if (count == 1 && in[0] <= 0) {
      fail_eval(machine, goal, "undefined");
      return -1;
    }
    return name == "log" ? unary(std::log) : unary(std::log2);
  }
  if (name == "atan2") {
    if (count != 2) return 0;
    out = std::atan2(in[0], in[1]);
    if (bad_float(machine, goal, out, true)) return -1;
    return 1;
  }
  if (name == "float" && count == 1) {
    out = in[0];
    return 1;
  }
  if (name == "copysign" && count == 2) {
    out = std::copysign(in[0], in[1]);
    return 1;
  }
  if (name == "nexttoward" && count == 2) {
    out = std::nexttoward(in[0], in[1]);
    if (bad_float(machine, goal, out, std::isfinite(in[0]))) return -1;
    return 1;
  }
  if (name == "float_integer_part" && count == 1) {
    out = std::trunc(in[0]);
    return 1;
  }
  if (name == "float_fractional_part" && count == 1) {
    out = in[0] - std::trunc(in[0]);
    return 1;
  }
  auto integral = [&](double value) {
    if (!to_int(machine, goal, value, iout)) return -1;
    as_int = true;
    return 1;
  };
  if ((name == "integer" || name == "truncate") && count == 1) return integral(std::trunc(in[0]));
  if (name == "floor" && count == 1) return integral(std::floor(in[0]));
  if (name == "ceiling" && count == 1) return integral(std::ceil(in[0]));
  if (name == "round" && count == 1) return integral(std::round(in[0]));
  return 0;
}

}
