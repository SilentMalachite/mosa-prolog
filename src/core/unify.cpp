#include "mosa/unify.hpp"

#include <bit>
#include <cstdint>
#include <vector>

namespace mosa {
namespace {

struct Attempt {
  const std::uint64_t* boundary = nullptr;
  std::vector<std::uint64_t*> slots;
};

bool is_integer(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

void bind_var(Heap& heap, Cell variable, Cell value, Attempt& attempt) {
  auto* slot = const_cast<std::uint64_t*>(heap.word(variable));
  // 選択点より新しい語は、ヒープ長を戻せば消える。古い語だけ戻せるように積む。
  if (slot < attempt.boundary) attempt.slots.push_back(slot);
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

bool unify_compound(Heap& heap, Cell left, Cell right, Attempt& attempt) {
  if (left.tag() == Tag::Lis) {
    return unify_cells(heap, heap.list_head(left), heap.list_head(right), attempt) &&
           unify_cells(heap, heap.list_tail(left), heap.list_tail(right), attempt);
  }
  if (heap.functor_arity(left) != heap.functor_arity(right)) return false;
  if (heap.functor_atom(left) != heap.functor_atom(right)) return false;
  return unify_args(heap, left, right, attempt);
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
  for (auto* slot : attempt.slots) *slot = std::bit_cast<std::uint64_t>(slot);
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
  trail.slots.insert(trail.slots.end(), attempt.slots.begin(), attempt.slots.end());
  return true;
}

}
