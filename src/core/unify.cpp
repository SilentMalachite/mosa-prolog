#include "mosa/unify.hpp"

#include <bit>
#include <cstdint>
#include <vector>

namespace mosa {
namespace {

struct Attempt {
  const std::uint64_t* boundary = nullptr;
  // 失敗時に戻す分。選択点より新しい節の変数も、呼び出し開始より前ならヒープ切り詰めでは消えない。
  std::vector<std::uint64_t*> undo;
  std::vector<std::uint64_t*> keep;
  // 循環を辿ると C++ の呼出が尽きる。同じ組を再訪したら、対応は既に付いている。
  std::vector<std::uint64_t> seen_left;
  std::vector<std::uint64_t> seen_right;
};

bool is_integer(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

void bind_var(Heap& heap, Cell variable, Cell value, Attempt& attempt) {
  auto* slot = const_cast<std::uint64_t*>(heap.word(variable));
  attempt.undo.push_back(slot);
  if (slot < attempt.boundary) attempt.keep.push_back(slot);
  heap.bind(variable, value);
}

bool unify_cells(Heap& heap, Cell left, Cell right, Attempt& attempt);

bool unify_args(Heap& heap, Cell left, Cell right, Attempt& attempt) {
  const auto arity = heap.functor_arity(left);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!unify_cells(heap, heap.arg(left, index), heap.arg(right, index), attempt)) return false;
  }
  return true;
}

bool already_seen(const Attempt& attempt, Cell left, Cell right) {
  for (std::size_t index = 0; index < attempt.seen_left.size(); ++index) {
    if (attempt.seen_left[index] == left.raw() && attempt.seen_right[index] == right.raw()) return true;
  }
  return false;
}

bool unify_compound(Heap& heap, Cell left, Cell right, Attempt& attempt) {
  if (already_seen(attempt, left, right)) return true;
  attempt.seen_left.push_back(left.raw());
  attempt.seen_right.push_back(right.raw());
  bool ok = false;
  if (left.tag() == Tag::Lis) {
    ok = unify_cells(heap, heap.list_head(left), heap.list_head(right), attempt) &&
         unify_cells(heap, heap.list_tail(left), heap.list_tail(right), attempt);
  } else if (heap.functor_arity(left) == heap.functor_arity(right) &&
             heap.functor_atom(left) == heap.functor_atom(right)) {
    ok = unify_args(heap, left, right, attempt);
  }
  attempt.seen_left.pop_back();
  attempt.seen_right.pop_back();
  return ok;
}

bool unify_cells(Heap& heap, Cell left, Cell right, Attempt& attempt) {
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
  if (left.tag() == Tag::Lis || left.tag() == Tag::Str) return unify_compound(heap, left, right, attempt);
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
  if (!unify_cells(heap, left, right, attempt)) {
    undo_attempt(heap, attempt, heap_mark);
    return false;
  }
  trail.slots.resize(trail_mark);
  trail.slots.insert(trail.slots.end(), attempt.keep.begin(), attempt.keep.end());
  return true;
}

}
