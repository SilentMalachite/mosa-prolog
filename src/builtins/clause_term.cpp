// 8.8.1 clause/2 が節の IR を項へ戻す。
// 同じスロットは一つのセル。匿名は出現ごとに新しい参照。
// ゴール位置の変数は call/1 に戻す。7.6.2 は変数ゴールを call として書く。
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

bool goal_connective(IrTag tag) {
  return tag == IrTag::And || tag == IrTag::Or || tag == IrTag::IfThen || tag == IrTag::SoftIf;
}

IrTerm call_of(Impl& machine, const IrTerm& inner) {
  IrTerm call;
  call.tag = IrTag::Fun;
  call.atom = machine.atoms.intern("call");
  call.arity = 1;
  call.args.push_back(inner);
  return call;
}

IrTerm expose(Impl& machine, const IrTerm& term, bool as_goal) {
  if (term.tag == IrTag::Call) {
    if (term.args.empty()) {
      IrTerm atom;
      atom.tag = IrTag::Atom;
      atom.atom = machine.atoms.intern("call");
      return atom;
    }
    // Call は呼び出し枠ではなく、中のゴールそのものを返す。
    return expose(machine, term.args[0], as_goal);
  }
  // 頭の変数はそのまま。ゴール位置だけ call/1 にする。
  if (as_goal && (term.tag == IrTag::Var || term.tag == IrTag::Anon)) return call_of(machine, term);
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
  if (goal_connective(term.tag)) {
    for (const IrTerm& arg : term.args) copy.args.push_back(expose(machine, arg, true));
    return copy;
  }
  if (term.tag == IrTag::Catch && term.args.size() >= 3) {
    copy.args.push_back(expose(machine, term.args[0], true));
    copy.args.push_back(expose(machine, term.args[1], false));
    copy.args.push_back(expose(machine, term.args[2], true));
    return copy;
  }
  for (const IrTerm& arg : term.args) copy.args.push_back(expose(machine, arg, false));
  return copy;
}

}  // namespace

Cell ir_to_term(Impl& machine, const IrTerm& term, std::vector<Cell>& slots, bool as_goal) {
  const IrTerm shown = expose(machine, term, as_goal);
  remember_slot(machine, shown, slots);
  return materialize(machine, shown, slots);
}

}  // namespace mosa
