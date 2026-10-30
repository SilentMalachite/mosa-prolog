#include "mosa/writer.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

std::uint32_t atom_of(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

bool is_integer(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

std::optional<std::string_view> var_name(std::span<const VarName> names, Cell cell) {
  for (const VarName& var : names) {
    if (var.cell.raw() == cell.raw()) return std::string_view{var.name};
  }
  return std::nullopt;
}

bool correspond(std::uint64_t left, std::uint64_t right, std::vector<std::pair<std::uint64_t, std::uint64_t>>& pairs) {
  for (const auto& [known_left, known_right] : pairs) {
    if (known_left == left || known_right == right) return known_left == left && known_right == right;
  }
  pairs.emplace_back(left, right);
  return true;
}

struct Side {
  const Heap& heap;
  std::span<const VarName> names;
};

using Pairs = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
// 比べ残しの組。C++ の再帰を使わず手で積む（ADR 0011）。積む順は比べる順の逆。
using Todo = std::vector<std::pair<Cell, Cell>>;

bool match_vars(const Side& left_side, Cell left, const Side& right_side, Cell right, Pairs& pairs) {
  if (!left_side.heap.unbound(left) || !right_side.heap.unbound(right)) return false;
  const auto left_label = var_name(left_side.names, left);
  const auto right_label = var_name(right_side.names, right);
  if (left_label || right_label) {
    if (!left_label || !right_label || *left_label != *right_label) return false;
  }
  // 名前の一致だけでは共有を見ない。同じ REF の再出現は、対応先も同じ REF にする。
  return correspond(left.raw(), right.raw(), pairs);
}

// 複合項の名前と引数の数を比べ、子の組を左から比べるよう逆順に積む。
bool push_children(const Side& left_side, Cell left, const Side& right_side, Cell right, Todo& todo) {
  const Heap& left_heap = left_side.heap;
  const Heap& right_heap = right_side.heap;
  if (left.tag() == Tag::Lis) {
    todo.emplace_back(left_heap.list_tail(left), right_heap.list_tail(right));
    todo.emplace_back(left_heap.list_head(left), right_heap.list_head(right));
    return true;
  }
  if (left_heap.functor_arity(left) != right_heap.functor_arity(right)) return false;
  const auto left_name = left_heap.atoms().name(left_heap.functor_atom(left));
  const auto right_name = right_heap.atoms().name(right_heap.functor_atom(right));
  if (left_name != right_name) return false;
  for (std::uint16_t index = left_heap.functor_arity(left); index > 0; --index) {
    todo.emplace_back(left_heap.arg(left, index - 1U), right_heap.arg(right, index - 1U));
  }
  return true;
}

bool match_one(const Side& left_side, Cell left, const Side& right_side, Cell right, Pairs& pairs, Todo& todo) {
  const Heap& left_heap = left_side.heap;
  const Heap& right_heap = right_side.heap;
  left = left_heap.deref(left);
  right = right_heap.deref(right);
  if (left_heap.unbound(left) || right_heap.unbound(right)) {
    return match_vars(left_side, left, right_side, right, pairs);
  }
  if (is_integer(left) && is_integer(right)) {
    return left_heap.integer_value(left) == right_heap.integer_value(right);
  }
  if (left.tag() != right.tag()) return false;
  if (left.tag() == Tag::Atom) return left_heap.atoms().name(atom_of(left)) == right_heap.atoms().name(atom_of(right));
  if (left.tag() == Tag::Float) return left_heap.float_value(left) == right_heap.float_value(right);
  if (left.tag() == Tag::Lis || left.tag() == Tag::Str) return push_children(left_side, left, right_side, right, todo);
  return false;
}

}  // namespace

bool same_term(const Heap& left_heap, Cell left, std::span<const VarName> left_names, const Heap& right_heap,
               Cell right, std::span<const VarName> right_names) {
  const Side left_side{left_heap, left_names};
  const Side right_side{right_heap, right_names};
  Pairs pairs;
  Todo todo{{left, right}};
  while (!todo.empty()) {
    const auto [left_cell, right_cell] = todo.back();
    todo.pop_back();
    if (!match_one(left_side, left_cell, right_side, right_cell, pairs, todo)) return false;
  }
  return true;
}

}
