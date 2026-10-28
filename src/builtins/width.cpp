// display_width(+integer, -integer)。ISO の条項は無い。
// library(format) の節からだけ応える。user から呼ぶと述語は無い。
// 東アジア幅の W と F は 2、それ以外は 1。
//   instantiation_error, type_error(integer)
// テスト: tests/pl/format.pl

#include "../machine/internal.hpp"

#include "mosa/char_class.hpp"

namespace mosa {

bool run_display_width(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "display_width" || goal.arity != 2) return false;
  // 公開述語にしない。書式の行幅だけが表を見る。
  if (machine.atoms.name(machine.calling) != "format") return false;
  const Cell code = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(code)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  if (code.tag() != Tag::Int && code.tag() != Tag::IntBox) {
    raise_formal(machine, formal_type(machine, "integer", code), goal.name, goal.arity);
    return true;
  }
  const std::int64_t value = machine.heap.integer_value(code);
  const bool wide = value >= 0 && value <= 0x10FFFF && east_asian_wide(static_cast<char32_t>(value));
  const Cell width = machine.heap.integer(wide ? 2 : 1);
  if (!do_unify(machine, goal.arg(machine, 1), width)) machine.failed = true;
  else machine.ip = goal.cont;
  return true;
}

}  // namespace mosa
