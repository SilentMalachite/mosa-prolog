// library(format) の下請けの内部述語。13211-1 に条項は無い。
// '$stream_column'(+stream_or_alias, -integer)。~N が使う。
//   Column は流れの最後の改行から書いた字の数で、0 から数える（stream_property/2 の
//   position(stream_position(_, _, LinePos)) の LinePos から 1 を引いた値）。字は Unicode のスカラー値 1 つを 1 と
//   数え、東アジア幅では数えない。
//   流れの検査は put_char/2 と同じ: instantiation_error、domain_error(stream_or_alias, S)、
//   existence_error(stream, S)、permission_error(output, stream, S)、permission_error(output, binary_stream, S)
// '$format_radix'(+integer, +integer, +atom, -atom)。~r と ~R が使う（フェーズ11のタスク7）。
//   Integer（多倍長も）を Radix 進（2〜36）で書いた原子。負なら '-' が先。Case が lower なら英字の桁は小文字、
//   upper なら大文字。
//   instantiation_error（どれかが未束縛）、type_error(integer, Integer)、type_error(integer, Radix)、
//   domain_error(radix, Radix)、domain_error(letter_case, Case)（原子でない Case も、lower と upper だけを受ける）、
//   system_error（std::to_chars が緩衝域に書けなかったとき。緩衝域は足りる大きさに取るので、届く入力は無い）
// テスト: tests/pl/lib_format.pl、tests/cpp/format_det_test.cpp
#include "../machine/internal.hpp"

#include "mosa/bigint.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace mosa {
namespace {

constexpr std::int64_t kRadixMin = 2;
constexpr std::int64_t kRadixMax = 36;

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

void prove_stream_column(Impl& machine, const Goal& goal) {
  std::size_t index = 0;
  if (select_output_stream(machine, goal, goal.arg(machine, 0), index)) return;
  // line_pos は行の頭で 1 になる（chars.cpp と write_pred.cpp の advance）。~N が知りたいのは行の頭かどうかなので、
  // 行の頭を 0 とする数え方で返す。
  const auto column = static_cast<std::int64_t>(machine.streams[index].line_pos - 1);
  if (do_unify(machine, goal.arg(machine, 1), machine.heap.integer(column))) machine.ip = goal.cont;
  else machine.failed = true;
}

std::optional<Cell> radix_problem(Impl& machine, Cell value, Cell radix, Cell letter_case) {
  if (machine.heap.unbound(value) || machine.heap.unbound(radix) || machine.heap.unbound(letter_case)) {
    return formal_instantiation(machine);
  }
  if (!is_integer_tag(value.tag())) return formal_type(machine, "integer", value);
  if (!is_integer_tag(radix.tag())) return formal_type(machine, "integer", radix);
  // integer_value は Big を受けない。Big の基数はどれも 36 を超えるか 2 より小さいので、値を見ずに範囲の外とする。
  if (radix.tag() == Tag::Big || machine.heap.integer_value(radix) < kRadixMin ||
      machine.heap.integer_value(radix) > kRadixMax) {
    return formal_domain(machine, "radix", radix);
  }
  if (letter_case.tag() != Tag::Atom) return formal_domain(machine, "letter_case", letter_case);
  const std::string_view name = machine.atoms.name(atom_id(letter_case));
  if (name != "lower" && name != "upper") return formal_domain(machine, "letter_case", letter_case);
  return std::nullopt;
}

// int64 は BigInt を作らずに to_chars で書く。最小の整数も 2 進で符号と 64 桁に収まる。どちらも英字は小文字で、
// 負なら '-' が先なので、Big と int64 で字面の形がそろう。to_chars が書けなかったときは nullopt（届く入力は無い）。
std::optional<std::string> radix_text(const Impl& machine, Cell value, int radix) {
  if (value.tag() == Tag::Big) return to_text(machine.heap.big_value(value), static_cast<unsigned>(radix));
  std::array<char, 65> buffer{};
  const std::int64_t number = machine.heap.integer_value(value);
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number, radix);
  // 失敗した to_chars は ptr に末尾を返す。そのまま組むと中身の決まらない緩衝域が原子になる。
  if (result.ec != std::errc{}) return std::nullopt;
  return std::string(buffer.data(), result.ptr);
}

// 桁の英字は ASCII の a〜z だけなので、ロケールに左右される toupper を使わない。
void make_upper(std::string& text) {
  for (char& c : text) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  }
}

void prove_format_radix(Impl& machine, const Goal& goal) {
  const Cell value = machine.heap.deref(goal.arg(machine, 0));
  const Cell radix = machine.heap.deref(goal.arg(machine, 1));
  const Cell letter_case = machine.heap.deref(goal.arg(machine, 2));
  if (const auto problem = radix_problem(machine, value, radix, letter_case)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  std::optional<std::string> text = radix_text(machine, value, static_cast<int>(machine.heap.integer_value(radix)));
  // 引数はどれも検査を通っており、ユーザが上げられる上限でもないので、処理系の内側の誤りとして system_error に
  // する（7.12.2 j）。resource_error は資源の名前を新たに作ることになる。束縛はまだ何もしていない。
  if (!text) {
    raise_formal(machine, formal_system(machine), goal.name, goal.arity);
    return;
  }
  if (machine.atoms.name(atom_id(letter_case)) == "upper") make_upper(*text);
  const Cell atom = machine.heap.atom(machine.atoms.intern(*text));
  if (do_unify(machine, goal.arg(machine, 3), atom)) machine.ip = goal.cont;
  else machine.failed = true;
}

}  // namespace

bool run_format_support(Impl& machine, const Goal& goal) {
  // どのゴールもここを通るので、名前を引く前に引数の数で外す。
  if (goal.arity != 2 && goal.arity != 4) return false;
  const std::string_view name = machine.atoms.name(goal.name);
  if (goal.arity == 2 && name == "$stream_column") {
    prove_stream_column(machine, goal);
    return true;
  }
  if (goal.arity == 4 && name == "$format_radix") {
    prove_format_radix(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
