// '$dcg_translate'(+term, ?term)。13211-1 に条項は無い（文法規則は 13211-3。ADR 0024 改訂 3）。
// library(dcg) の expand_term/2 の実体。文法規則 H --> B を 1 つ訳し、規則でない項はそのまま返す。
//   instantiation_error、type_error(callable, Culprit)、type_error(list, Culprit)
// テスト: tests/pl/lib_dcg.pl
#include "../machine/dcg.hpp"
#include "../machine/internal.hpp"

namespace mosa {
namespace {

bool is_rule_arrow(const Impl& machine, Cell term) {
  return term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(term)) == "-->";
}

}  // namespace

bool run_dcg_translate(Impl& machine, const Goal& goal) {
  const Cell term = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(term)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  Cell result = term;
  if (is_rule_arrow(machine, term)) {
    const DcgOut translated = dcg_rule(machine, term);
    if (translated.formal) {
      raise_formal(machine, *translated.formal, goal.name, goal.arity);
      return true;
    }
    result = translated.term;
  }
  // 束縛は最後に 1 回だけ。失敗したら戻して、途中の束縛を残さない。
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  const auto extras_mark = machine.extras.size();
  if (!do_unify(machine, result, goal.arg(machine, 1))) {
    restore_state(machine, heap_mark, trail_mark, extras_mark);
    machine.failed = true;
    return true;
  }
  machine.ip = goal.cont;
  return true;
}

}  // namespace mosa
