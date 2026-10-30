#include "mosa/unify.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct PairHash {
  std::size_t operator()(const std::pair<std::uint64_t, std::uint64_t>& pair) const {
    return std::hash<std::uint64_t>{}(pair.first ^ (pair.second * 0x9E3779B97F4A7C15ULL));
  }
};

// 辿った複合項の組を表に入れ始めるまでの数。小さい単一化では表を作らない。巡回する項もこの数の後は
// 同じ組を 2 度辿らないので止まる。
constexpr std::size_t kSeenAfter = 256;
// この深さまでは C++ の再帰で子を辿る。より深い部分木は、その場で手で積んで辿り終えてから戻る（ADR 0011）。
// 浅い単一化は確保を伴わず、辿る順は前の再帰と同じになる。
constexpr int kShallow = 64;

struct Attempt {
  const std::uint64_t* boundary = nullptr;
  // 失敗時に戻す分。選択点より新しい節の変数も、呼び出し開始より前ならヒープ切り詰めでは消えない。
  std::vector<std::uint64_t*> undo;
  std::vector<std::uint64_t*> keep;
  // この単一化で辿った複合項の組。祖先に限らず全部を覚えても、余帰納的に同じ答えになる。
  std::unordered_set<std::pair<std::uint64_t, std::uint64_t>, PairHash> seen;
  std::size_t compounds = 0;
};

using Todo = std::vector<std::pair<Cell, Cell>>;

bool is_integer(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

void bind_var(Heap& heap, Cell variable, Cell value, Attempt& attempt) {
  auto* slot = const_cast<std::uint64_t*>(heap.word(variable));
  attempt.undo.push_back(slot);
  if (slot < attempt.boundary) attempt.keep.push_back(slot);
  heap.bind(variable, value);
}

bool unify_cells(Heap& heap, Cell left, Cell right, Attempt& attempt, int depth);

// 部分木を手で積んで辿る。子は左から比べるよう逆順に積む。
bool unify_deep(Heap& heap, Cell left, Cell right, Attempt& attempt) {
  Todo todo{{left, right}};
  while (!todo.empty()) {
    const auto [next_left, next_right] = todo.back();
    todo.pop_back();
    const Cell l = heap.deref(next_left);
    const Cell r = heap.deref(next_right);
    // 同じ種類の別の複合項の組だけをここで開く。それ以外は unify_cells がその場で決め、再び降りない。
    const bool open = (l.tag() == Tag::Lis || l.tag() == Tag::Str) && l.tag() == r.tag() && l.raw() != r.raw();
    if (!open) {
      if (!unify_cells(heap, l, r, attempt, kShallow)) return false;
      continue;
    }
    if (++attempt.compounds > kSeenAfter && !attempt.seen.emplace(l.raw(), r.raw()).second) continue;
    if (l.tag() == Tag::Lis) {
      todo.emplace_back(heap.list_tail(l), heap.list_tail(r));
      todo.emplace_back(heap.list_head(l), heap.list_head(r));
      continue;
    }
    if (heap.functor_arity(l) != heap.functor_arity(r) || heap.functor_atom(l) != heap.functor_atom(r)) return false;
    for (std::uint16_t index = heap.functor_arity(l); index > 0; --index) {
      todo.emplace_back(heap.arg(l, index - 1U), heap.arg(r, index - 1U));
    }
  }
  return true;
}

bool unify_compound(Heap& heap, Cell left, Cell right, Attempt& attempt, int depth) {
  if (depth >= kShallow) return unify_deep(heap, left, right, attempt);
  if (++attempt.compounds > kSeenAfter && !attempt.seen.emplace(left.raw(), right.raw()).second) return true;
  if (left.tag() == Tag::Lis) {
    return unify_cells(heap, heap.list_head(left), heap.list_head(right), attempt, depth + 1) &&
           unify_cells(heap, heap.list_tail(left), heap.list_tail(right), attempt, depth + 1);
  }
  if (heap.functor_arity(left) != heap.functor_arity(right) || heap.functor_atom(left) != heap.functor_atom(right)) {
    return false;
  }
  const auto arity = heap.functor_arity(left);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!unify_cells(heap, heap.arg(left, index), heap.arg(right, index), attempt, depth + 1)) return false;
  }
  return true;
}

bool unify_cells(Heap& heap, Cell left, Cell right, Attempt& attempt, int depth) {
  left = heap.deref(left);
  right = heap.deref(right);
  // NaN は自分自身とも一致しない。浮動小数は番地が同じでも == に回す。
  if (left.raw() == right.raw() && left.tag() != Tag::Float) return true;
  if (heap.unbound(left)) {
    bind_var(heap, left, right, attempt);
    return true;
  }
  if (heap.unbound(right)) {
    bind_var(heap, right, left, attempt);
    return true;
  }
  if (is_integer(left) && is_integer(right)) return heap.integer_value(left) == heap.integer_value(right);
  if (left.tag() != right.tag()) return false;
  if (left.tag() == Tag::Float) return heap.float_value(left) == heap.float_value(right);
  if (left.tag() == Tag::Lis || left.tag() == Tag::Str) return unify_compound(heap, left, right, attempt, depth);
  return false;
}

void undo_attempt(Heap& heap, Attempt& attempt, std::size_t heap_mark) {
  for (auto* slot : attempt.undo) *slot = std::bit_cast<std::uint64_t>(slot);
  heap.truncate(heap_mark);
}

}  // namespace

bool unify(Heap& heap, Trail& trail, std::size_t trail_mark, std::size_t heap_mark, Cell left, Cell right) {
  Attempt attempt;
  attempt.boundary = trail.mark != nullptr ? trail.mark : heap.data() + heap_mark;
  if (!unify_cells(heap, left, right, attempt, 0)) {
    undo_attempt(heap, attempt, heap_mark);
    return false;
  }
  trail.slots.resize(trail_mark);
  trail.slots.insert(trail.slots.end(), attempt.keep.begin(), attempt.keep.end());
  return true;
}

}
