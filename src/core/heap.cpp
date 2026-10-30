#include "mosa/heap.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace mosa {
namespace {

constexpr std::int64_t k_min_immediate = -(std::int64_t{1} << 60);
constexpr std::int64_t k_max_immediate = std::int64_t{1} << 60;

std::uint64_t tagged(std::uint64_t payload, Tag tag) {
  return payload | static_cast<std::uint64_t>(tag);
}

// 伸ばす単位は 1 MiB（語で 2^17）。小刻みに mprotect/VirtualAlloc を呼ばないための粒度。
constexpr std::size_t k_commit_step = std::size_t{1} << 17;

AddressRange reserve_or_throw(std::size_t limit) {
  // 語数からバイト数への掛け算があふれる上限は、予約できないものとして同じ止め方にする。
  if (limit > std::numeric_limits<std::size_t>::max() / sizeof(std::uint64_t)) throw ResourceLimit("heap_limit");
  auto range = AddressRange::reserve(limit * sizeof(std::uint64_t));
  if (!range) throw ResourceLimit("heap_limit");
  return std::move(*range);
}

}  // namespace

Heap::Heap(AtomTable& atoms, std::size_t limit)
    : atoms_(atoms), limit_(limit), range_(reserve_or_throw(limit)) {
  grow(std::min(kInitialHeapCommit, limit));
}

void Heap::grow(std::size_t need) {
  const std::size_t rounded = (need + k_commit_step - 1) / k_commit_step * k_commit_step;
  const std::size_t target = std::min(rounded, limit_);
  if (target < need || !range_.commit(target * sizeof(std::uint64_t))) throw ResourceLimit("heap_limit");
  committed_words_ = target;
}

void Heap::grow_for(std::size_t count) {
  if (count > limit_ || used_ > limit_ - count) throw ResourceLimit("heap_limit");
  grow(used_ + count);
}

// どちらも used_ <= committed_words_ <= limit_（allocate の速い道が頼る不変条件）を壊さない大きさだけを渡すこと。
void Heap::truncate(std::size_t size) {
  assert(size <= committed_words_);
  used_ = size;
}

void Heap::reset_size_after_gc(std::size_t used) {
  assert(used <= committed_words_);
  used_ = used;
}

std::uint64_t Heap::word_at(std::size_t index) const { return range_.words()[index]; }

void Heap::set_word(std::size_t index, std::uint64_t value) { range_.words()[index] = value; }

const std::uint64_t* Heap::pointer(Cell cell) const {
  const auto address = cell.raw() & ~std::uint64_t{7};
  return std::bit_cast<const std::uint64_t*>(address);
}

const std::uint64_t* Heap::word(Cell cell) const { return pointer(cell); }

Cell Heap::fresh_ref() {
  auto* slot = allocate(1);
  const auto address = std::bit_cast<std::uint64_t>(slot);
  *slot = address;
  return Cell{address};
}

bool Heap::unbound(Cell cell) const {
  return cell.tag() == Tag::Ref && *pointer(cell) == cell.raw();
}

Cell Heap::deref(Cell cell) const {
  while (cell.tag() == Tag::Ref) {
    const Cell next{*pointer(cell)};
    if (next.raw() == cell.raw()) return cell;
    cell = next;
  }
  return cell;
}

void Heap::bind(Cell variable, Cell value) {
  // 未束縛のスロットはヒープの語そのもの。const の番地計算から書き戻す。
  auto* slot = const_cast<std::uint64_t*>(pointer(variable));
  *slot = value.raw();
}

Cell Heap::atom(std::uint32_t id) const {
  return Cell{tagged(static_cast<std::uint64_t>(id) << 3, Tag::Atom)};
}

Cell Heap::integer(std::int64_t value) {
  // 61bit 符号付きに収まる値だけ即値。2^60 はその範囲の外なので箱にする。
  if (value >= k_min_immediate && value < k_max_immediate) {
    return Cell{tagged(static_cast<std::uint64_t>(value) << 3, Tag::Int)};
  }
  auto* slot = allocate(1);
  *slot = std::bit_cast<std::uint64_t>(value);
  return Cell{tagged(std::bit_cast<std::uint64_t>(slot), Tag::IntBox)};
}

std::int64_t Heap::integer_value(Cell cell) const {
  if (cell.tag() == Tag::Int) {
    const auto cleared = static_cast<std::int64_t>(cell.raw() & ~std::uint64_t{7});
    return cleared >> 3;
  }
  return std::bit_cast<std::int64_t>(*pointer(cell));
}

Cell Heap::floating(double value) {
  auto* slot = allocate(1);
  *slot = std::bit_cast<std::uint64_t>(value);
  return Cell{tagged(std::bit_cast<std::uint64_t>(slot), Tag::Float)};
}

double Heap::float_value(Cell cell) const { return std::bit_cast<double>(*pointer(cell)); }

Cell Heap::structure(std::uint32_t functor, std::span<const Cell> args) {
  auto* slot = allocate(1 + args.size());
  slot[0] = pack_functor(functor, static_cast<std::uint16_t>(args.size()));
  for (std::size_t index = 0; index < args.size(); ++index) slot[index + 1] = args[index].raw();
  return Cell{tagged(std::bit_cast<std::uint64_t>(slot), Tag::Str)};
}

std::uint16_t Heap::functor_arity(Cell cell) const { return mosa::functor_arity(*pointer(cell)); }

std::uint32_t Heap::functor_atom(Cell cell) const { return functor_atom_id(*pointer(cell)); }

Cell Heap::arg(Cell cell, std::size_t index) const { return Cell{pointer(cell)[index + 1]}; }

Cell Heap::list(Cell head, Cell tail) {
  auto* slot = allocate(2);
  slot[0] = head.raw();
  slot[1] = tail.raw();
  return Cell{tagged(std::bit_cast<std::uint64_t>(slot), Tag::Lis)};
}

Cell Heap::list_head(Cell cell) const { return Cell{*pointer(cell)}; }

Cell Heap::list_tail(Cell cell) const { return Cell{pointer(cell)[1]}; }

}
