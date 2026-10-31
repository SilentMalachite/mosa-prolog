// 8.8.1 clause/2 が節の IR を項へ戻す。
// 同じスロットは一つのセル。匿名は出現ごとに新しい参照。
// ゴール位置の変数は call/1 に戻す。7.6.2 は変数ゴールを call として書く。
// テスト: tests/pl/clause.pl

#include "../machine/internal.hpp"

#include <string_view>
#include <tuple>
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

// 7.6.2 b が変換するのはゴール位置の , ; -> の引数だけ。c により catch/3 の引数はそのまま見せる。
// catch の中の (X, a) を (call(X), a) にすると、assert した形で retract できない。
bool child_as_goal(const IrTerm& term, bool term_as_goal) {
  return term_as_goal && goal_connective(term.tag);
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
  bool as_goal;
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
  frames.push_back(ExposeFrame{term, std::move(copy), 0, as_goal});
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
    const bool goal = child_as_goal(*top.source, top.as_goal);
    IrTerm leaf;
    // expose_start が frames を伸ばすと top は無効になるので、葉は改めて末尾の枠へ足す。
    if (expose_start(machine, child, goal, leaf, frames)) frames.back().built.args.push_back(std::move(leaf));
  }
}

// expose_start がこの節点自身の形を変えるか。
bool node_differs(const IrTerm& term, bool as_goal) {
  if (term.tag == IrTag::Call) return true;
  if (as_goal && (term.tag == IrTag::Var || term.tag == IrTag::Anon)) return true;
  if (term.tag == IrTag::Halt && term.args.empty()) return true;
  return shown_count(term) != term.args.size();
}

// 子をゴールとして訳すのは compile.cpp の goal_frame の 5 つだけ。他の葉の引数はデータで形が変わらない。
bool has_goal_children(const IrTerm& term) { return goal_connective(term.tag) || term.tag == IrTag::Catch; }

}  // namespace

// expose_start が形を変える節点があるか。無ければ写さずに戻せる。
// retract は候補ごとに本体を組むので、写しの確保が大きな本体で 3 倍の時間になった。
// 連言は右へ入れ子になるので、最後の子へは積まずに進む。普通の本体では確保が起きない。
bool shown_differs(const IrTerm& root, bool as_goal) {
  std::vector<std::pair<const IrTerm*, bool>> pending;
  const IrTerm* term = &root;
  bool goal = as_goal;
  while (true) {
    if (node_differs(*term, goal)) return true;
    if (has_goal_children(*term) && !term->args.empty()) {
      const bool child_goal = child_as_goal(*term, goal);
      for (std::size_t index = 0; index + 1 < term->args.size(); ++index) {
        const IrTerm& arg = term->args[index];
        if (has_goal_children(arg)) pending.emplace_back(&arg, child_goal);
        else if (node_differs(arg, child_goal)) return true;
      }
      term = &term->args.back();
      goal = child_goal;
      continue;
    }
    if (pending.empty()) return false;
    std::tie(term, goal) = pending.back();
    pending.pop_back();
  }
}

Cell ir_to_term(Impl& machine, const IrTerm& term, std::vector<Cell>& slots, bool as_goal) {
  if (!shown_differs(term, as_goal)) {
    remember_slot(machine, term, slots);
    return materialize(machine, term, slots);
  }
  const IrTerm shown = expose(machine, term, as_goal);
  remember_slot(machine, shown, slots);
  return materialize(machine, shown, slots);
}

}  // namespace mosa
