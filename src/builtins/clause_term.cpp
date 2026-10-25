// 8.8.1 clause/2 が節の IR を項へ戻す。
// 同じスロットは一つのセル。匿名は出現ごとに新しい参照。
// テスト: tests/pl/clause.pl

#include "../machine/internal.hpp"

#include <vector>

namespace mosa {
namespace {

void remember_slot(Impl& machine, const IrTerm& term, std::vector<Cell>& slots) {
  if (term.tag == IrTag::Var) {
    // fresh_ref はヒープ番地なので 0 ではない。0 はまだ作っていない穴。
    if (slots.size() <= term.slot) slots.resize(static_cast<std::size_t>(term.slot) + 1);
    if (slots[term.slot].raw() == 0) slots[term.slot] = machine.heap.fresh_ref();
    return;
  }
  for (const IrTerm& arg : term.args) remember_slot(machine, arg, slots);
}

IrTerm expose(Impl& machine, const IrTerm& term) {
  if (term.tag == IrTag::Call) {
    if (term.args.empty()) {
      IrTerm atom;
      atom.tag = IrTag::Atom;
      atom.atom = machine.atoms.intern("call");
      return atom;
    }
    // Call は呼び出し枠ではなく、中のゴールそのものを返す。
    return expose(machine, term.args[0]);
  }
  if (term.tag == IrTag::Halt && term.args.empty()) {
    // materialize は引数 0 個でも構造体にする。halt/0 はアトム halt である。
    IrTerm atom;
    atom.tag = IrTag::Atom;
    atom.atom = machine.atoms.intern("halt");
    return atom;
  }
  IrTerm copy = term;
  copy.args.clear();
  copy.args.reserve(term.args.size());
  for (const IrTerm& arg : term.args) copy.args.push_back(expose(machine, arg));
  return copy;
}

}  // namespace

Cell ir_to_term(Impl& machine, const IrTerm& term, std::vector<Cell>& slots) {
  const IrTerm shown = expose(machine, term);
  remember_slot(machine, shown, slots);
  return materialize(machine, shown, slots);
}

}  // namespace mosa
