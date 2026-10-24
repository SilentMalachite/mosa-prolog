#include "mosa/unify.hpp"

#include <bit>
#include <cstdint>
#include <vector>

namespace mosa {
namespace {

struct Trail {
  const std::uint64_t* boundary = nullptr;
  std::vector<std::uint64_t*> slots;
};

bool is_integer(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

void bind_var(Heap& heap, Cell variable, Cell value, Trail& trail) {
  auto* slot = const_cast<std::uint64_t*>(heap.word(variable));
  // 今回の呼び出しより前の語だけ戻す。呼び出し後の語はヒープ長を戻せば消える。
  if (slot < trail.boundary) trail.slots.push_back(slot);
  heap.bind(variable, value);
}

bool unify_cells(Heap& heap, Cell left, Cell right, Trail& trail);

bool unify_args(Heap& heap, Cell left, Cell right, Trail& trail) {
  const auto arity = heap.functor_arity(left);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!unify_cells(heap, heap.arg(left, index), heap.arg(right, index), trail)) return false;
  }
  return true;
}

bool unify_compound(Heap& heap, Cell left, Cell right, Trail& trail) {
  if (left.tag() == Tag::Lis) {
    return unify_cells(heap, heap.list_head(left), heap.list_head(right), trail) &&
           unify_cells(heap, heap.list_tail(left), heap.list_tail(right), trail);
  }
  if (heap.functor_arity(left) != heap.functor_arity(right)) return false;
  if (heap.functor_atom(left) != heap.functor_atom(right)) return false;
  return unify_args(heap, left, right, trail);
}

bool unify_cells(Heap& heap, Cell left, Cell right, Trail& trail) {
  left = heap.deref(left);
  right = heap.deref(right);
  // NaN は自分自身とも一致しない。浮動小数は番地が同じでも == に回す。
  if (left.raw() == right.raw() && left.tag() != Tag::Float) return true;
  if (heap.unbound(left)) {
    bind_var(heap, left, right, trail);
    return true;
  }
  if (heap.unbound(right)) {
    bind_var(heap, right, left, trail);
    return true;
  }
  if (is_integer(left) && is_integer(right)) return heap.integer_value(left) == heap.integer_value(right);
  if (left.tag() != right.tag()) return false;
  if (left.tag() == Tag::Float) return heap.float_value(left) == heap.float_value(right);
  if (left.tag() == Tag::Lis || left.tag() == Tag::Str) return unify_compound(heap, left, right, trail);
  return false;
}

void undo(Heap& heap, Trail& trail, std::size_t mark) {
  for (auto* slot : trail.slots) *slot = std::bit_cast<std::uint64_t>(slot);
  heap.truncate(mark);
}

}  // namespace

bool unify(Heap& heap, Cell left, Cell right) {
  const auto mark = heap.size();
  Trail trail;
  trail.boundary = heap.data() + mark;
  if (unify_cells(heap, left, right, trail)) return true;
  undo(heap, trail, mark);
  return false;
}

}
