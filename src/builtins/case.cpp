// '$upcase_atom'(+atom, ?atom)。13211-1 に条項は無い。library(strings) の upcase_atom/2 が使う。
//   1 文字ずつ単純大文字化（UnicodeData.txt の 12 列目）で写すので、文字数は変わらない。
//   instantiation_error、type_error(atom, Atom)
// テスト: tests/pl/lib_strings.pl、tests/cpp/char_class_test.cpp
#include "../machine/internal.hpp"
#include "mosa/char_class.hpp"

#include <cstdint>
#include <string>

namespace mosa {
namespace {

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

void prove_upcase_atom(Impl& machine, const Goal& goal) {
  const Cell atom = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(atom)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  if (atom.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", atom), goal.name, goal.arity);
    return;
  }
  const CodeText text = decode_codes(machine.atoms.name(atom_id(atom)));
  std::string upper;
  upper.reserve(text.bytes.size());
  for (const char32_t code : text.codes) append_utf8_code(upper, simple_upper(code));
  const Cell result = machine.heap.atom(machine.atoms.intern(upper));
  if (do_unify(machine, goal.arg(machine, 1), result)) machine.ip = goal.cont;
  else machine.failed = true;
}

}  // namespace

bool run_case(Impl& machine, const Goal& goal) {
  if (goal.arity != 2 || machine.atoms.name(goal.name) != "$upcase_atom") return false;
  prove_upcase_atom(machine, goal);
  return true;
}

}  // namespace mosa
