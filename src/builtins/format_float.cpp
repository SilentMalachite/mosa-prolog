// '$format_float'(+atom, +integer, +float, -atom)。13211-1 に条項は無い。library(format) の ~f と ~e が使う。
//   Kind は f（小数点表記）か e（指数表記）、Digits は小数点以下の桁数。
//   instantiation_error、type_error(atom, Kind)、domain_error(format_directive, Kind)、
//   type_error(integer, Digits)、domain_error(not_less_than_zero, Digits)、type_error(float, Float)
// テスト: tests/pl/lib_format.pl
#include "../machine/internal.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>

namespace mosa {
namespace {

// double の 2 進展開は小数点以下 1074 桁、有効数字 767 桁で尽きる。それより先の桁は 0 なので、
// to_chars には上限までを頼み、残りは 0 を足す。巨大な桁数でも緩衝域を桁数ぶん取らずに済む。
constexpr std::int64_t kExactDigits = 1100;

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

std::optional<Cell> float_problem(Impl& machine, Cell kind, Cell digits, Cell value) {
  if (machine.heap.unbound(kind) || machine.heap.unbound(digits) || machine.heap.unbound(value)) {
    return formal_instantiation(machine);
  }
  if (kind.tag() != Tag::Atom) return formal_type(machine, "atom", kind);
  const auto name = machine.atoms.name(atom_id(kind));
  if (name != "f" && name != "e") return formal_domain(machine, "format_directive", kind);
  if (!is_int(digits)) return formal_type(machine, "integer", digits);
  if (machine.heap.integer_value(digits) < 0) return formal_domain(machine, "not_less_than_zero", digits);
  if (value.tag() != Tag::Float) return formal_type(machine, "float", value);
  return std::nullopt;
}

std::string format_float(double value, bool scientific, std::int64_t digits) {
  const auto precision = static_cast<int>(std::min(digits, kExactDigits));
  // 小数点の前は最大 309 桁、指数は 5 文字、符号と小数点で 2 文字。
  std::string text(static_cast<std::size_t>(precision) + 330, '\0');
  const auto format = scientific ? std::chars_format::scientific : std::chars_format::fixed;
  const auto result = std::to_chars(text.data(), text.data() + text.size(), value, format, precision);
  text.resize(static_cast<std::size_t>(result.ptr - text.data()));
  if (digits > precision) {
    const auto zeros = static_cast<std::size_t>(digits - precision);
    const auto at = scientific ? text.find('e') : text.size();
    text.insert(at, zeros, '0');
  }
  return text;
}

void prove_format_float(Impl& machine, const Goal& goal) {
  const Cell kind = machine.heap.deref(goal.arg(machine, 0));
  const Cell digits = machine.heap.deref(goal.arg(machine, 1));
  const Cell value = machine.heap.deref(goal.arg(machine, 2));
  if (const auto problem = float_problem(machine, kind, digits, value)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  const bool scientific = machine.atoms.name(atom_id(kind)) == "e";
  const std::string text =
      format_float(machine.heap.float_value(value), scientific, machine.heap.integer_value(digits));
  const Cell atom = machine.heap.atom(machine.atoms.intern(text));
  if (do_unify(machine, goal.arg(machine, 3), atom)) machine.ip = goal.cont;
  else machine.failed = true;
}

}  // namespace

bool run_format_float(Impl& machine, const Goal& goal) {
  if (goal.arity != 4 || machine.atoms.name(goal.name) != "$format_float") return false;
  prove_format_float(machine, goal);
  return true;
}

}  // namespace mosa
