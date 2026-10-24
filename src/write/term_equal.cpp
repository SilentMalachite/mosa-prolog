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

bool match(const Heap& left_heap, Cell left, std::span<const VarName> left_names, const Heap& right_heap,
           Cell right, std::span<const VarName> right_names,
           std::vector<std::pair<std::uint64_t, std::uint64_t>>& pairs);

bool match_compound(const Heap& left_heap, Cell left, std::span<const VarName> left_names,
                    const Heap& right_heap, Cell right, std::span<const VarName> right_names,
                    std::vector<std::pair<std::uint64_t, std::uint64_t>>& pairs) {
  if (left.tag() == Tag::Lis) {
    return match(left_heap, left_heap.list_head(left), left_names, right_heap, right_heap.list_head(right),
                 right_names, pairs) &&
           match(left_heap, left_heap.list_tail(left), left_names, right_heap, right_heap.list_tail(right),
                 right_names, pairs);
  }
  if (left_heap.functor_arity(left) != right_heap.functor_arity(right)) return false;
  const auto left_name = left_heap.atoms().name(left_heap.functor_atom(left));
  const auto right_name = right_heap.atoms().name(right_heap.functor_atom(right));
  if (left_name != right_name) return false;
  const auto arity = left_heap.functor_arity(left);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!match(left_heap, left_heap.arg(left, index), left_names, right_heap, right_heap.arg(right, index),
               right_names, pairs)) {
      return false;
    }
  }
  return true;
}

bool match_vars(const Heap& left_heap, Cell left, std::span<const VarName> left_names, const Heap& right_heap,
                Cell right, std::span<const VarName> right_names,
                std::vector<std::pair<std::uint64_t, std::uint64_t>>& pairs) {
  if (!left_heap.unbound(left) || !right_heap.unbound(right)) return false;
  const auto left_label = var_name(left_names, left);
  const auto right_label = var_name(right_names, right);
  if (left_label || right_label) {
    if (!left_label || !right_label || *left_label != *right_label) return false;
  }
  // 名前の一致だけでは共有を見ない。同じ REF の再出現は、対応先も同じ REF にする。
  return correspond(left.raw(), right.raw(), pairs);
}

bool match(const Heap& left_heap, Cell left, std::span<const VarName> left_names, const Heap& right_heap,
           Cell right, std::span<const VarName> right_names,
           std::vector<std::pair<std::uint64_t, std::uint64_t>>& pairs) {
  left = left_heap.deref(left);
  right = right_heap.deref(right);
  if (left_heap.unbound(left) || right_heap.unbound(right)) {
    return match_vars(left_heap, left, left_names, right_heap, right, right_names, pairs);
  }
  if (is_integer(left) && is_integer(right)) {
    return left_heap.integer_value(left) == right_heap.integer_value(right);
  }
  if (left.tag() != right.tag()) return false;
  if (left.tag() == Tag::Atom) return left_heap.atoms().name(atom_of(left)) == right_heap.atoms().name(atom_of(right));
  if (left.tag() == Tag::Float) return left_heap.float_value(left) == right_heap.float_value(right);
  if (left.tag() == Tag::Lis || left.tag() == Tag::Str) {
    return match_compound(left_heap, left, left_names, right_heap, right, right_names, pairs);
  }
  return false;
}

}  // namespace

bool same_term(const Heap& left_heap, Cell left, std::span<const VarName> left_names, const Heap& right_heap,
               Cell right, std::span<const VarName> right_names) {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> pairs;
  return match(left_heap, left, left_names, right_heap, right, right_names, pairs);
}

}
