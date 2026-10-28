// upcase_atom(+atom, ?atom)。ISO の条項は無い。
// system の組込。library(strings) が再公開する。user から修飾なしで呼ぶと述語は無い。
// 単純大文字写像。写像の無い符号位置はそのまま。
//   instantiation_error, type_error(atom)
// テスト: tests/pl/strings.pl

#include "../machine/internal.hpp"

#include "mosa/char_class.hpp"

namespace mosa {
namespace {

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

void type_atom(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "atom", culprit), goal.name, goal.arity);
}

std::string mapped_text(const Impl& machine, Cell atom) {
  const CodeText codes = decode_codes(machine.atoms.name(atom_id(atom)));
  std::string text;
  for (const char32_t code : codes.codes) {
    append_utf8_code(text, simple_uppercase(code));
  }
  return text;
}

}  // namespace

bool run_upcase_atom(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "upcase_atom" || goal.arity != 2) return false;
  // 表は system だけが見る。ライブラリが修飾して再公開する。
  if (machine.atoms.name(machine.calling) != "system") return false;
  const Cell source = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(source)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (source.tag() != Tag::Atom) {
    type_atom(machine, goal, source);
    return true;
  }
  const Cell upper = machine.heap.deref(goal.arg(machine, 1));
  if (!machine.heap.unbound(upper) && upper.tag() != Tag::Atom) {
    type_atom(machine, goal, upper);
    return true;
  }
  const Cell made = machine.heap.atom(machine.atoms.intern(mapped_text(machine, source)));
  if (!do_unify(machine, goal.arg(machine, 1), made)) machine.failed = true;
  else machine.ip = goal.cont;
  return true;
}

}  // namespace mosa
