// '$char_width'(+integer, ?integer)。13211-1 に条項は無い。library(japanese) の 読み幅/2 が使う。
//   East_Asian_Width が W か F の文字は 2、それ以外は 1。結合文字や制御文字も 1 に数える。
//   instantiation_error、type_error(integer, Code)、representation_error(character_code)（Code が Big でも。フェーズ10）
// テスト: tests/pl/japanese.pl、tests/pl/bigint.pl
#include "../machine/internal.hpp"
#include "mosa/char_class.hpp"

namespace mosa {
namespace {

void prove_char_width(Impl& machine, const Goal& goal) {
  const Cell code = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(code)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  if (!is_integer_tag(code.tag())) {
    raise_formal(machine, formal_type(machine, "integer", code), goal.name, goal.arity);
    return;
  }
  // Big はどの符号でも Unicode の範囲の外にあり、範囲の外の int64 と同じく文字コードではない。
  if (code.tag() == Tag::Big || !scalar_code(machine.heap.integer_value(code))) {
    raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
    return;
  }
  const int width = east_asian_wide(static_cast<char32_t>(machine.heap.integer_value(code))) ? 2 : 1;
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
