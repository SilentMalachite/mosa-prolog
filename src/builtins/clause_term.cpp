// 8.8.1 clause/2 が節の IR を項へ戻す。
// 同じスロットは一つのセル。匿名は出現ごとに新しい参照。
// ゴール位置の変数は call/1 に戻す。7.6.2 は変数ゴールを call として書く。
// テスト: tests/pl/clause.pl

#include "../machine/internal.hpp"

#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// 前の再帰と同じ左からの前順に穴を作る。変数の標準順序は番地で決まるので、順を変えると答えが変わる。
// 深い節でも C++ のスタックを使わないよう、まだ見ていない項を手で積む（ADR 0011）。
void remember_slot(Impl& machine, const IrTerm& root, std::vector<Cell>& slots) {
  std::vector<const IrTerm*> pending{&root};
  while (!pending.empty()) {
    const IrTerm& term = *pending.back();
    pending.pop_back();
    if (term.tag == IrTag::Var) {
      // fresh_ref はヒープ番地なので 0 ではない。0 はまだ作っていない穴。
      if (slots.size() <= term.slot) slots.resize(static_cast<std::size_t>(term.slot) + 1);
      if (slots[term.slot].raw() == 0) slots[term.slot] = machine.heap.fresh_ref();
      continue;
    }
    for (auto arg = term.args.rbegin(); arg != term.args.rend(); ++arg) pending.push_back(&*arg);
  }
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

// 子の数と、i 番目の子をゴールとして見せるか。catch/3 は 3 つだけ残す。
std::size_t shown_count(const IrTerm& term) {
  if (term.tag == IrTag::Catch && term.args.size() >= 3) return 3;
  return term.args.size();
}

bool child_as_goal(const IrTerm& term, std::size_t index) {
  if (goal_connective(term.tag)) return true;
  if (term.tag == IrTag::Catch && term.args.size() >= 3) return index != 1;
  return false;
}

IrTerm named_atom(Impl& machine, std::string_view name) {
  IrTerm atom;
  atom.tag = IrTag::Atom;
  atom.atom = machine.atoms.intern(name);
  return atom;
}

// 見せる途中の項 1 つ分。built は子を除いた写しで、子は見せ終えたものから足す。
struct ExposeFrame {
  const IrTerm* source;
  IrTerm built;
  std::size_t next;
};

// 子を持たない形になったものは out に置いて true を返す。子を持つ項は frames に積んで false を返す。
bool expose_start(Impl& machine, const IrTerm* term, bool as_goal, IrTerm& out, std::vector<ExposeFrame>& frames) {
  // Call は呼び出し枠ではなく、中のゴールそのものを返す。
  while (term->tag == IrTag::Call && !term->args.empty()) term = &term->args[0];
  if (term->tag == IrTag::Call) {
    out = named_atom(machine, "call");
    return true;
  }
  // 頭の変数はそのまま。ゴール位置だけ call/1 にする。
  if (as_goal && (term->tag == IrTag::Var || term->tag == IrTag::Anon)) {
    out = call_of(machine, *term);
    return true;
  }
  // materialize は引数 0 個でも構造体にする。halt/0 はアトム halt である。
  if (term->tag == IrTag::Halt && term->args.empty()) {
    out = named_atom(machine, "halt");
    return true;
  }
  // 子ごと写すと深い項で 2 乗の手間になるので、子を除いた欄だけを写す。
  IrTerm copy;
  copy.tag = term->tag;
  copy.slot = term->slot;
  copy.atom = term->atom;
  copy.arity = term->arity;
  copy.integer = term->integer;
  copy.floating = term->floating;
  if (shown_count(*term) == 0) {
    out = std::move(copy);
    return true;
  }
  copy.args.reserve(shown_count(*term));
  frames.push_back(ExposeFrame{term, std::move(copy), 0});
  return false;
}

// 深い節でも C++ のスタックを使わないよう、祖先を frames に手で積む（ADR 0011）。
IrTerm expose(Impl& machine, const IrTerm& root, bool as_goal) {
  std::vector<ExposeFrame> frames;
  IrTerm done;
  if (expose_start(machine, &root, as_goal, done, frames)) return done;
  while (true) {
    ExposeFrame& top = frames.back();
    if (top.next == shown_count(*top.source)) {
      done = std::move(top.built);
      frames.pop_back();
      if (frames.empty()) return done;
      frames.back().built.args.push_back(std::move(done));
      continue;
    }
    const std::size_t index = top.next++;
    const IrTerm* child = &top.source->args[index];
    const bool goal = child_as_goal(*top.source, index);
    IrTerm leaf;
    // expose_start が frames を伸ばすと top は無効になるので、葉は改めて末尾の枠へ足す。
    if (expose_start(machine, child, goal, leaf, frames)) frames.back().built.args.push_back(std::move(leaf));
  }
}

}  // namespace

Cell ir_to_term(Impl& machine, const IrTerm& term, std::vector<Cell>& slots, bool as_goal) {
  const IrTerm shown = expose(machine, term, as_goal);
  remember_slot(machine, shown, slots);
  return materialize(machine, shown, slots);
}

}  // namespace mosa
