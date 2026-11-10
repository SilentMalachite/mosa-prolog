// 型検査（8.3）と unify_with_occurs_check/2（8.2.2）、is_list/1。
// ground/1 は Cor.2 の 8.3.10。ground(?term)。エラーは投げない。テストは tests/pl/types.pl、tests/pl/unify.pl、
// tests/stack/deep_builtins.pl。
// acyclic_term/1 は Cor.2 の 8.3.11。acyclic_term(@term)。エラーは投げない。テストは tests/pl/types.pl。

#include "../machine/internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mosa {
namespace {

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

bool is_atomic(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom || is_int(cell) || cell.tag() == Tag::Float;
}

bool is_compound(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

bool is_nil(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom && machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3)) == "[]";
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

// 祖先に戻る道があれば循環。辿り終えた複合項（done）は共有されていても辿り直さない。前は辿った項すべてを
// 祖先として扱い、f(X, X) の共有を循環と取り違えていた（8.2.2）。C++ の再帰を使わず手で積む（ADR 0011）。
// 根を全部積んでも、1 つの根を辿り終えてから次の根に移るので path は根ごとに空から始まる。done は根の間で
// 共有し、束縛の多い unify_with_occurs_check/2 でも項を 1 度しか辿らない（レビュー L3 の敵対的レビュー A3）。
bool acyclic_terms(const Impl& machine, std::span<const Cell> roots) {
  std::unordered_set<std::uint64_t> path;
  std::unordered_set<std::uint64_t> done;
  // 2 番目が真の項目は「この複合項の子を辿り終えた」の印。
  std::vector<std::pair<Cell, bool>> pending;
  pending.reserve(roots.size());
  for (const Cell root : roots) pending.emplace_back(root, false);
  while (!pending.empty()) {
    const auto [cell, leave] = pending.back();
    pending.pop_back();
    if (leave) {
      path.erase(cell.raw());
      done.insert(cell.raw());
      continue;
    }
    const Cell term = machine.heap.deref(cell);
    if (term.tag() != Tag::Lis && term.tag() != Tag::Str) continue;
    if (path.contains(term.raw())) return false;
    if (done.contains(term.raw())) continue;
    path.insert(term.raw());
    pending.emplace_back(term, true);
    if (term.tag() == Tag::Lis) {
      pending.emplace_back(machine.heap.list_tail(term), false);
      pending.emplace_back(machine.heap.list_head(term), false);
      continue;
    }
    for (std::uint16_t index = machine.heap.functor_arity(term); index > 0; --index) {
      pending.emplace_back(machine.heap.arg(term, index - 1U), false);
    }
  }
  return true;
}

// この数までの複合項は集合を作らずに辿る。小さい項の ground/1 の費用を増やさないため。大きい項では集合が
// 複合項 1 つに数十バイトを --heap-max の外に使う（is_list/1 の proper_list と同じ）。
constexpr std::size_t kGroundPlain = 1024;

// 深い項で C++ のスタックを使い切らないよう、再帰ではなく明示のスタックで辿る。束縛は一切しない。
// kGroundPlain 個を超えたら辿った複合項を覚えて二度辿らない。巡回する項は止まらず、f(X, X) を重ねた
// 共有の多い項は指数に膨らんでいた（レビュー M7）。覚える前に辿った複合項は高々もう一度辿るだけである。
bool ground_term(const Impl& machine, Cell cell) {
  std::vector<Cell> pending{cell};
  std::unordered_set<std::uint64_t> seen;
  std::size_t steps = 0;
  while (!pending.empty()) {
    const Cell current = machine.heap.deref(pending.back());
    pending.pop_back();
    if (machine.heap.unbound(current)) return false;
    if (current.tag() != Tag::Lis && current.tag() != Tag::Str) continue;
    if (++steps > kGroundPlain && !seen.insert(current.raw()).second) continue;
    if (current.tag() == Tag::Lis) {
      pending.push_back(machine.heap.list_tail(current));
      pending.push_back(machine.heap.list_head(current));
      continue;
    }
    for (std::uint16_t index = 0; index < machine.heap.functor_arity(current); ++index) {
      pending.push_back(machine.heap.arg(current, index));
    }
  }
  return true;
}

bool proper_list(const Impl& machine, Cell cell) {
  std::unordered_set<std::uint64_t> spine;
  Cell cursor = cell;
  while (true) {
    cursor = machine.heap.deref(cursor);
    if (machine.heap.unbound(cursor)) return false;
    if (is_nil(machine, cursor)) break;
    if (cursor.tag() != Tag::Lis) return false;
    if (!spine.insert(cursor.raw()).second) return false;
    cursor = machine.heap.list_tail(cursor);
  }
  // 背骨だけを見る。要素の循環はリストであることを変えない。前は項全体を acyclic_term で調べ、format/2 と msort/2 が
  // 循環する要素のリストを受け取れなかった（レビュー L3）。
  return true;
}

// 8.2.2 の「NSTO なら実装依存」を、束縛した変数の値が循環すれば失敗とする（iso-notes 8.2.2 の決定 B）。
// 結果全体の循環を見ると、前から循環している X = f(X), Y = f(Y) の X と Y のように何も束縛しない組まで
// 失敗した（レビュー L3）。束縛のたびに出現を調べると、同じ方程式でも束縛の順で答えが変わった（敵対的レビュー A1）。
// 束縛した値だけを辿るので、失敗する単一化は前と同じくすぐ失敗する（同 A2）。
void occurs_unify(Impl& machine, const Goal& goal) {
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  const auto extras_mark = machine.extras.size();
  std::vector<Cell> bound;
  if (!do_unify(machine, goal.arg(machine, 0), goal.arg(machine, 1), bound)) {
    machine.failed = true;
    return;
  }
  // 選択点より新しい変数はトレイルに残らないが、失敗の後戻りがヒープごと捨てる。
  if (!acyclic_terms(machine, bound)) {
    restore_state(machine, heap_mark, trail_mark, extras_mark);
    machine.failed = true;
    return;
  }
  succeed(machine, goal);
}

// 循環項を作れるので、unify_with_occurs_check/2 と同じ明示スタックの acyclic_terms で判定する（ADR 0011）。
bool acyclic_one(const Impl& machine, Cell cell) {
  const std::array<Cell, 1> roots{cell};
  return acyclic_terms(machine, roots);
}

bool typed(Impl& machine, const Goal& goal, bool (*test)(const Impl&, Cell)) {
  if (test(machine, goal.arg(machine, 0))) succeed(machine, goal);
  else machine.failed = true;
  return true;
}

}  // namespace

bool run_types(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (goal.arity != 1 && !(goal.arity == 2 && (name == "unify_with_occurs_check"))) return false;
  if (name == "var" && goal.arity == 1) {
    if (machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "nonvar" && goal.arity == 1) {
    if (!machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "atom" && goal.arity == 1) {
    const Cell value = machine.heap.deref(goal.arg(machine, 0));
    if (value.tag() == Tag::Atom) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "integer" && goal.arity == 1) {
    if (is_int(machine.heap.deref(goal.arg(machine, 0)))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "float" && goal.arity == 1) {
    if (machine.heap.deref(goal.arg(machine, 0)).tag() == Tag::Float) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "atomic" && goal.arity == 1) return typed(machine, goal, is_atomic);
  if (name == "compound" && goal.arity == 1) return typed(machine, goal, is_compound);
  if (name == "number" && goal.arity == 1) {
    const Cell value = machine.heap.deref(goal.arg(machine, 0));
    if (is_int(value) || value.tag() == Tag::Float) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "callable" && goal.arity == 1) {
    // Cor.2 の 8.3.9 は型検査である。変数も数も失敗し、例外は投げない。
    const Cell value = machine.heap.deref(goal.arg(machine, 0));
    if (value.tag() == Tag::Atom || is_compound(machine, value)) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "ground" && goal.arity == 1) return typed(machine, goal, ground_term);
  if (name == "acyclic_term" && goal.arity == 1) return typed(machine, goal, acyclic_one);
  if (name == "is_list" && goal.arity == 1) return typed(machine, goal, proper_list);
  if (name == "unify_with_occurs_check" && goal.arity == 2) {
    occurs_unify(machine, goal);
    return true;
  }
  return false;
}

}
