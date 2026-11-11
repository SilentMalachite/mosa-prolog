// 8.2.4 subsumes_term(@term, @term)（Cor.2）。General が Specific を包摂するとき真。エラー無し。
// 成功しても失敗しても束縛を残さない。循環した項は docs/iso-notes.md の 8.2.4。
// テスト: tests/pl/unify.pl
#include "../machine/internal.hpp"

#include <vector>

namespace mosa {
namespace {

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

// 単一化の前後で Specific の変数が同じ未束縛の変数のままか。
// 何かが束縛されていたら、General が Specific の変数を具体化しようとしたことになる。
bool variables_unchanged(const Impl& machine, const std::vector<Cell>& before, const std::vector<Cell>& after) {
  if (before.size() != after.size()) return false;
  for (std::size_t index = 0; index < before.size(); ++index) {
    const Cell now = machine.heap.deref(after[index]);
    if (!machine.heap.unbound(now) || machine.heap.deref(before[index]).raw() != now.raw()) return false;
  }
  return true;
}

}  // namespace

bool run_subsumes_term(Impl& machine, const Goal& goal) {
  const Cell general = goal.arg(machine, 0);
  const Cell specific = goal.arg(machine, 1);
  const std::vector<Cell> before = term_variable_cells(machine, specific);
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  const auto extras_mark = machine.extras.size();
  // unify_with_occurs_check/2 は使わない。8.2.2 の決定 B ですでに循環した項への束縛も失敗するため、
  // X = f(X) の後の subsumes_term(f(_), f(X)) が偽になり、SWI-Prolog と準拠スイートに合わない（iso-notes 8.2.4）。
  // 循環していない入力では、既定の単一化と変数の並びの比較で真偽は同じになる。
  bool subsumes = do_unify(machine, general, specific);
  if (subsumes) subsumes = variables_unchanged(machine, before, term_variable_cells(machine, specific));
  restore_state(machine, heap_mark, trail_mark, extras_mark);
  if (!subsumes) {
    machine.failed = true;
    return true;
  }
  succeed(machine, goal);
  return true;
}

}  // namespace mosa
