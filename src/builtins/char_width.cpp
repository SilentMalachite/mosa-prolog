// '$char_width'(+integer, ?integer)。13211-1 に条項は無い。library(japanese) の 読み幅/2 が使う。
//   East_Asian_Width が W か F の文字は 2、それ以外は 1。結合文字や制御文字も 1 に数える。
//   instantiation_error、type_error(integer, Code)、representation_error(character_code)
// テスト: tests/pl/japanese.pl
#include "../machine/internal.hpp"
#include "mosa/char_class.hpp"

namespace mosa {
namespace {

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

void prove_char_width(Impl& machine, const Goal& goal) {
  const Cell code = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(code)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  if (!is_int(code)) {
    raise_formal(machine, formal_type(machine, "integer", code), goal.name, goal.arity);
    return;
  }
  const auto value = machine.heap.integer_value(code);
  if (!scalar_code(value)) {
    raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
    return;
  }
  const int width = east_asian_wide(static_cast<char32_t>(value)) ? 2 : 1;
  if (do_unify(machine, goal.arg(machine, 1), machine.heap.integer(width))) machine.ip = goal.cont;
  else machine.failed = true;
}

}  // namespace

bool run_char_width(Impl& machine, const Goal& goal) {
  if (goal.arity != 2 || machine.atoms.name(goal.name) != "$char_width") return false;
  prove_char_width(machine, goal);
  return true;
}

}  // namespace mosa
