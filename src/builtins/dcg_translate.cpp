// '$dcg_translate'(+term, ?term)。13211-1 に条項は無い（文法規則は 13211-3。ADR 0024 改訂 3）。
// library(dcg) の expand_term/2 の実体。文法規則 H --> B を 1 つ訳し、規則でない項はそのまま返す。
// エラー:
//   instantiation_error、type_error(callable, Culprit)、type_error(list, Culprit)
// テスト: tests/pl/lib_dcg.pl
#include "../machine/dcg.hpp"
#include "../machine/internal.hpp"

namespace mosa {

bool run_dcg_translate(Impl& machine, const Goal& goal) {
  const Cell term = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(term)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return true;
  }
  Cell result = term;
  if (is_rule_arrow(machine.heap, term)) {
    const DcgOut translated = dcg_rule(machine, term);
    if (translated.formal) {
      raise_formal(machine, *translated.formal, goal.name, goal.arity);
      return true;
    }
    result = translated.term;
  }
  // 束縛は最後に 1 回だけ。失敗しても、選択点より新しい変数はトレイルに残らないが、後戻りがヒープごと捨てる。
  if (!do_unify(machine, result, goal.arg(machine, 1))) {
    machine.failed = true;
    return true;
  }
  machine.ip = goal.cont;
  return true;
}

}  // namespace mosa
