// データベース述語の修飾。database.cpp を 800 行に収めるため、ここへ分ける。
// Module:Clause と (Module:Head :- Body)。未知のモジュールは existence_error(module)。
// テスト: tests/pl/modules.pl

#include "../machine/internal.hpp"

#include <cstdint>
#include <vector>

namespace mosa {
namespace {

// 0 は修飾ではない。1 は剥がした。-1 はモジュールの誤りを送った。
int take_qualified(Impl& machine, const Goal& goal, Cell term, std::uint32_t& home, Cell& inner) {
  term = machine.heap.deref(term);
  if (term.tag() != Tag::Str || machine.heap.functor_arity(term) != 2) return 0;
  if (machine.atoms.name(machine.heap.functor_atom(term)) != ":") return 0;
  const Cell module = machine.heap.deref(machine.heap.arg(term, 0));
  // 変数は型より先。未束縛を type_error にしない。
  if (machine.heap.unbound(module)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return -1;
  }
  if (module.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", module), goal.name, goal.arity);
    return -1;
  }
  const auto named = static_cast<std::uint32_t>(module.raw() >> 3);
  if (!module_loaded(machine, named)) {
    raise_formal(machine, formal_existence(machine, "module", module), goal.name, goal.arity);
    return -1;
  }
  home = named;
  inner = machine.heap.deref(machine.heap.arg(term, 1));
  return 1;
}

// (Module:Head :- Body) は、Head の節を Module に足す。外側が :- なので事実の剥がしでは届かない。
int peel_rule_head(Impl& machine, const Goal& goal, Cell& term, std::uint32_t& home) {
  const Cell looked = machine.heap.deref(term);
  if (looked.tag() != Tag::Str || machine.heap.functor_arity(looked) != 2) return 0;
  if (machine.atoms.name(machine.heap.functor_atom(looked)) != ":-") return 0;
  Cell bare;
  const int found = take_qualified(machine, goal, machine.heap.arg(looked, 0), home, bare);
  if (found <= 0) return found;
  const std::vector<Cell> args{bare, machine.heap.arg(looked, 1)};
  term = machine.heap.structure(machine.atoms.intern(":-"), args);
  return 1;
}

}  // namespace

bool directive_pi_home(Impl& machine, Cell& arg, std::uint32_t& home) {
  const Cell term = machine.heap.deref(arg);
  if (term.tag() != Tag::Str || machine.heap.functor_arity(term) != 2) return true;
  if (machine.atoms.name(machine.heap.functor_atom(term)) != ":") return true;
  const Cell module = machine.heap.deref(machine.heap.arg(term, 0));
  if (machine.heap.unbound(module) || module.tag() != Tag::Atom) return false;
  const auto named = static_cast<std::uint32_t>(module.raw() >> 3);
  if (!module_loaded(machine, named)) return false;
  home = named;
  arg = machine.heap.deref(machine.heap.arg(term, 1));
  return true;
}

bool place_clause(Impl& machine, const Goal& goal, Cell term, std::uint32_t& home, Cell& placed) {
  home = machine.calling;
  Cell inner;
  const int outer = take_qualified(machine, goal, term, home, inner);
  if (outer < 0) return false;
  if (outer > 0) term = inner;
  if (peel_rule_head(machine, goal, term, home) < 0) return false;
  placed = term;
  return true;
}

}  // namespace mosa
