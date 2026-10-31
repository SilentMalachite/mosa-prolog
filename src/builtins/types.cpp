// 型検査（8.3）と unify_with_occurs_check/2（8.2.2）、is_list/1。
// ground/1 は Cor.2 の 8.3.10。ground(?term)。エラーは投げない。テストは tests/pl/types.pl。

#include "../machine/internal.hpp"

#include <cstdint>
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
bool acyclic_term(const Impl& machine, Cell root) {
  std::unordered_set<std::uint64_t> path;
  std::unordered_set<std::uint64_t> done;
  // 2 番目が真の項目は「この複合項の子を辿り終えた」の印。
  std::vector<std::pair<Cell, bool>> pending{{root, false}};
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

// 深い項で C++ のスタックを使い切らないよう、再帰ではなく明示のスタックで辿る。束縛は一切しない。
bool ground_term(const Impl& machine, Cell cell) {
  std::vector<Cell> pending{cell};
  while (!pending.empty()) {
    const Cell current = machine.heap.deref(pending.back());
    pending.pop_back();
    if (machine.heap.unbound(current)) return false;
    if (current.tag() == Tag::Lis) {
      pending.push_back(machine.heap.list_tail(current));
      pending.push_back(machine.heap.list_head(current));
      continue;
    }
    if (current.tag() != Tag::Str) continue;
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
  return acyclic_term(machine, cell);
}

void occurs_unify(Impl& machine, const Goal& goal) {
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  const auto extras_mark = machine.extras.size();
  if (!do_unify(machine, goal.arg(machine, 0), goal.arg(machine, 1))) {
    machine.failed = true;
    return;
  }
  // 選択点より新しい変数はトレイルに残らない。束縛後の項が循環なら失敗させる。
  const bool cyclic = !acyclic_term(machine, goal.arg(machine, 0)) || !acyclic_term(machine, goal.arg(machine, 1));
  if (cyclic) {
    restore_state(machine, heap_mark, trail_mark, extras_mark);
    machine.failed = true;
    return;
  }
  succeed(machine, goal);
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
  if (name == "ground" && goal.arity == 1) {
    if (ground_term(machine, goal.arg(machine, 0))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "is_list" && goal.arity == 1) {
    if (proper_list(machine, goal.arg(machine, 0))) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "unify_with_occurs_check" && goal.arity == 2) {
    occurs_unify(machine, goal);
    return true;
  }
  return false;
}

}
