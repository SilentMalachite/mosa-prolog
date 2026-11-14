// '$format_float'(+atom, +integer, +float, -atom)。13211-1 に条項は無い。library(format) の ~f ~e ~E ~g ~G が使う。
//   Kind は f（小数点表記）、e（指数表記）、E（e の指数の字を E にする）、g（C の %g と同じく、指数の大きさで f と e を
//   選び、末尾の 0 を落とす）、G（g の指数の字を E にする）。Digits は f、e、E では小数点以下の桁数、g と G では
//   有効数字の桁数（0 は 1 と同じ）。無限大と NaN は Kind と Digits によらず writeq と同じ 1.0Inf、-1.0Inf、1.5NaN に
//   なる。f、e、E の有限の数で Digits がヒープの上限の語数を超えれば、ヒープの上限で止まる（ResourceLimit）。
//   g と G の字面は桁数によらず 1100 字ほどで尽きるので、止めずに作る。
//   instantiation_error、type_error(atom, Kind)、domain_error(format_directive, Kind)、
//   type_error(integer, Digits)、domain_error(not_less_than_zero, Digits)、type_error(float, Float)
//   Digits の Big（フェーズ10、設計の 8）は、負なら domain_error、正なら f、e、E では大きすぎる int64 と同じくヒープの
//   上限で止まり、g と G では 1100 桁と同じ字面になる。
// テスト: tests/pl/lib_format.pl、tests/pl/bigint.pl、tests/cpp/bigint_limit_test.cpp、tests/cpp/format_digits_test.cpp
#include "../machine/internal.hpp"
#include "../write/spelling.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mosa {
namespace {

// double の 2 進展開は小数点以下 1074 桁、有効数字 767 桁で尽きる。それより先の桁は 0 なので、
// to_chars には上限までを頼み、残りは 0 を足す。巨大な桁数でも緩衝域を桁数ぶん取らずに済む。
constexpr std::int64_t kExactDigits = 1100;

// 指示の字から to_chars の書式と指数の字の大きさを決める。E と G は e と g の字面の e だけを E に変える。
struct FloatKind {
  std::chars_format format;
  bool upper;
};

std::optional<FloatKind> float_kind(std::string_view name) {
  if (name == "f") return FloatKind{std::chars_format::fixed, false};
  if (name == "e") return FloatKind{std::chars_format::scientific, false};
  if (name == "E") return FloatKind{std::chars_format::scientific, true};
  if (name == "g") return FloatKind{std::chars_format::general, false};
  if (name == "G") return FloatKind{std::chars_format::general, true};
  return std::nullopt;
}

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

std::optional<Cell> float_problem(Impl& machine, Cell kind, Cell digits, Cell value) {
  if (machine.heap.unbound(kind) || machine.heap.unbound(digits) || machine.heap.unbound(value)) {
    return formal_instantiation(machine);
  }
  if (kind.tag() != Tag::Atom) return formal_type(machine, "atom", kind);
  if (!float_kind(machine.atoms.name(atom_id(kind)))) return formal_domain(machine, "format_directive", kind);
  if (!is_integer_tag(digits.tag())) return formal_type(machine, "integer", digits);
  if (integer_negative(machine, digits)) return formal_domain(machine, "not_less_than_zero", digits);
  if (value.tag() != Tag::Float) return formal_type(machine, "float", value);
  return std::nullopt;
}

std::string format_float(double value, FloatKind kind, std::int64_t digits) {
  // to_chars の無限大と NaN は標準ライブラリごとに字面が違い（-nan(ind) など）、指数の e も無いので
  // 0 を足す位置も無い。writeq と同じ字面にして、桁数によらず一つの字面にする（レビュー M29、iso-notes 6.4.5）。
  // 大文字の指示でもここで返すので、Inf と NaN の字は大文字にならない。
  if (!std::isfinite(value)) return spelling::format_float(value);
  const auto precision = static_cast<int>(std::min(digits, kExactDigits));
  // 小数点の前は最大 309 桁、指数は 5 文字、符号と小数点で 2 文字。g の小数点表記の先頭の 0 も 330 字に収まる。
  std::string text(static_cast<std::size_t>(precision) + 330, '\0');
  const auto result = std::to_chars(text.data(), text.data() + text.size(), value, kind.format, precision);
  text.resize(static_cast<std::size_t>(result.ptr - text.data()));
  // g は末尾の 0 を落とす書式なので、上限より先の桁は足しても落ちる。足さない。g の小数点表記には e が無く、
  // find が npos を返すので、e を探す前に g を外す（npos への insert は out_of_range で落ちる）。
  if (kind.format != std::chars_format::general && digits > precision) {
    const auto zeros = static_cast<std::size_t>(digits - precision);
    const auto at = kind.format == std::chars_format::scientific ? text.find('e') : text.size();
    text.insert(at, zeros, '0');
  }
  // 0 を足してから e を探す。有限の数の字面で英字は指数の e だけで、g の小数点表記には e が無い。
  if (const auto at = text.find('e'); kind.upper && at != std::string::npos) text[at] = 'E';
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
  const FloatKind style = *float_kind(machine.atoms.name(atom_id(kind)));
  const double number = machine.heap.float_value(value);
  // format.pl は答えの原子を atom_chars で字のリストにし、1 字にヒープの 2 語を使う（ADR 0001）。f、e、E で上限の
  // 語数より多い桁は、そのリストが必ず上限を超える。その前にヒープの外へ巨大な文字列を作らず、ここで上限に止める。
  // 2^63 - 1 桁では string::insert が length_error でプロセスごと落ちた（M29 の Codex 敵対的レビュー）。
  // 正の Big の桁数はどの上限より多い。無限大と NaN は桁数を使わないので、Big でも字面は 1 つに決まる。
  // g と G は 0 を足さず、字面は上限の桁 kExactDigits で尽きるので止めない。Big の桁も kExactDigits と同じ字面になる。
  const bool big = digits.tag() == Tag::Big;
  const std::int64_t count = big ? kExactDigits : machine.heap.integer_value(digits);
  const bool padded = style.format != std::chars_format::general;
  if (std::isfinite(number) && padded && (big || static_cast<std::uint64_t>(count) > machine.heap.limit())) {
    throw ResourceLimit("heap_limit");
  }
  const std::string text = format_float(number, style, count);
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
