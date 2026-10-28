// ひらがな_カタカナ(?atom, ?atom)、全角_半角(?atom, ?atom)、読み幅(+atom, ?integer)。
// ISO の条項は無い。system の組込。library(japanese) が再公開する。
// user から修飾なしで呼ぶと述語は無い。
// かなは U+3041..U+3096 と U+30A1..U+30F6 を 0x60 で往復する。範囲外はそのまま。
// 全角_半角は空白と ASCII のブロック差、および半角カタカナの互換分解。対が無ければそのまま。
// 読み幅は東アジア幅の W と F を 2、それ以外を 1 とし、アトムの幅はその和。
//   instantiation_error, type_error(atom), type_error(integer)
// テスト: tests/pl/japanese.pl

#include "../machine/internal.hpp"

#include "mosa/char_class.hpp"

#include <string>
#include <string_view>

namespace mosa {
namespace {

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

void type_error(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

std::int64_t text_width(const Impl& machine, Cell atom) {
  const CodeText codes = decode_codes(machine.atoms.name(atom_id(atom)));
  std::int64_t width = 0;
  for (const char32_t code : codes.codes) width += east_asian_wide(code) ? 2 : 1;
  return width;
}

char32_t to_katakana(char32_t code) {
  if (code >= 0x3041 && code <= 0x3096) return code + 0x60;
  return code;
}

char32_t to_hiragana(char32_t code) {
  if (code >= 0x30A1 && code <= 0x30F6) return code - 0x60;
  return code;
}

char32_t to_halfwidth(char32_t code) {
  if (code == 0x3000) return 0x20;
  if (code >= 0xFF01 && code <= 0xFF5E) return code - 0xFEE0;
  return halfwidth_partner(code, false);
}

char32_t to_fullwidth(char32_t code) {
  if (code == 0x20) return 0x3000;
  if (code >= 0x21 && code <= 0x7E) return code + 0xFEE0;
  return halfwidth_partner(code, true);
}

std::string map_text(const Impl& machine, Cell atom, char32_t (*map)(char32_t)) {
  const CodeText codes = decode_codes(machine.atoms.name(atom_id(atom)));
  std::string text;
  for (const char32_t code : codes.codes) append_utf8_code(text, map(code));
  return text;
}

bool atom_or_var(Impl& machine, const Goal& goal, Cell cell) {
  if (machine.heap.unbound(cell) || cell.tag() == Tag::Atom) return true;
  type_error(machine, goal, "atom", cell);
  return false;
}

bool map_pair(Impl& machine, const Goal& goal, char32_t (*to_right)(char32_t),
              char32_t (*to_left)(char32_t)) {
  const Cell left = machine.heap.deref(goal.arg(machine, 0));
  const Cell right = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(left) && machine.heap.unbound(right)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (!atom_or_var(machine, goal, left) || !atom_or_var(machine, goal, right)) return true;
  const bool from_right = machine.heap.unbound(left);
  const Cell source = from_right ? right : left;
  const std::string mapped = map_text(machine, source, from_right ? to_left : to_right);
  const Cell made = machine.heap.atom(machine.atoms.intern(mapped));
  if (!do_unify(machine, goal.arg(machine, from_right ? 0 : 1), made)) machine.failed = true;
  else machine.ip = goal.cont;
  return true;
}

bool system_builtin(const Impl& machine, const Goal& goal, std::string_view name) {
  return machine.atoms.name(goal.name) == name && goal.arity == 2 &&
         machine.atoms.name(machine.calling) == "system";
}

}  // namespace

bool run_yomi_width(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "読み幅" || goal.arity != 2) return false;
  // 表は system だけが見る。ライブラリが修飾して再公開する。
  if (machine.atoms.name(machine.calling) != "system") return false;
  const Cell text = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(text)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (text.tag() != Tag::Atom) {
    type_error(machine, goal, "atom", text);
    return true;
  }
  const Cell width = machine.heap.deref(goal.arg(machine, 1));
  if (!machine.heap.unbound(width) && width.tag() != Tag::Int && width.tag() != Tag::IntBox) {
    type_error(machine, goal, "integer", width);
    return true;
  }
  const Cell made = machine.heap.integer(text_width(machine, text));
  if (!do_unify(machine, goal.arg(machine, 1), made)) machine.failed = true;
  else machine.ip = goal.cont;
  return true;
}

bool run_kana(Impl& machine, const Goal& goal) {
  // 表は system だけが見る。ライブラリが修飾して再公開する。
  if (!system_builtin(machine, goal, "ひらがな_カタカナ")) return false;
  return map_pair(machine, goal, to_katakana, to_hiragana);
}

bool run_zen_han(Impl& machine, const Goal& goal) {
  if (!system_builtin(machine, goal, "全角_半角")) return false;
  return map_pair(machine, goal, to_halfwidth, to_fullwidth);
}

}  // namespace mosa
