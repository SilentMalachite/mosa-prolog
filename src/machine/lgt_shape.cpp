// 包摂は同じ変数セルを同じ項にする。lgt_file が 800 行を超える前に分けた。

#include "lgt_suite.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

std::string_view atom_text(const Heap& heap, Cell cell) {
  return heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

bool integer_cell(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox;
}

// 一度合わせた項の再出現は、この形と変数セルで見る。名前の無い same_term は別の自由変数を同じにしてしまう。
bool same_shape(const Heap& heap, Cell left, Cell right) {
  left = heap.deref(left);
  right = heap.deref(right);
  if (heap.unbound(left) || heap.unbound(right)) {
    return heap.unbound(left) && heap.unbound(right) && left.raw() == right.raw();
  }
  if (integer_cell(heap, left) && integer_cell(heap, right)) {
    return heap.integer_value(left) == heap.integer_value(right);
  }
  if (left.tag() != right.tag()) return false;
  if (left.tag() == Tag::Atom) return atom_text(heap, left) == atom_text(heap, right);
  if (left.tag() == Tag::Float) return heap.float_value(left) == heap.float_value(right);
  if (left.tag() == Tag::Lis) {
    return same_shape(heap, heap.list_head(left), heap.list_head(right)) &&
           same_shape(heap, heap.list_tail(left), heap.list_tail(right));
  }
  if (left.tag() != Tag::Str || heap.functor_arity(left) != heap.functor_arity(right)) return false;
  if (heap.atoms().name(heap.functor_atom(left)) != heap.atoms().name(heap.functor_atom(right))) return false;
  const auto arity = heap.functor_arity(left);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!same_shape(heap, heap.arg(left, index), heap.arg(right, index))) return false;
  }
  return true;
}

struct NumberValue {
  bool integer = false;
  std::int64_t as_int = 0;
  double as_float = 0.0;
};

bool fill_number(const Heap& heap, Cell cell, bool negative, NumberValue& out) {
  cell = heap.deref(cell);
  if (integer_cell(heap, cell)) {
    auto value = heap.integer_value(cell);
    if (negative) {
      if (value == std::numeric_limits<std::int64_t>::min()) return false;
      value = -value;
    }
    out.integer = true;
    out.as_int = value;
    return true;
  }
  if (cell.tag() != Tag::Float) return false;
  out.integer = false;
  out.as_float = negative ? -heap.float_value(cell) : heap.float_value(cell);
  return true;
}

// 数そのものか、その前の + と - だけ。式は評価しない。
std::optional<NumberValue> signed_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  NumberValue out;
  if (fill_number(heap, cell, false, out)) return out;
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "+" && name != "-") return std::nullopt;
  if (!fill_number(heap, heap.arg(cell, 0), name == "-", out)) return std::nullopt;
  return out;
}

bool sign_of_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "+" && name != "-") return false;
  const Cell arg = heap.deref(heap.arg(cell, 0));
  return integer_cell(heap, arg) || arg.tag() == Tag::Float;
}

bool same_number(const NumberValue& left, const NumberValue& right) {
  if (left.integer != right.integer) return false;
  if (left.integer) return left.as_int == right.as_int;
  return left.as_float == right.as_float;
}

bool subsumes(const Heap& heap, Cell general, Cell specific, std::vector<std::pair<std::uint64_t, Cell>>& bound) {
  general = heap.deref(general);
  specific = heap.deref(specific);
  if (heap.unbound(general)) {
    for (const auto& [id, value] : bound) {
      if (id == general.raw()) return same_shape(heap, value, specific);
    }
    bound.emplace_back(general.raw(), specific);
    return true;
  }
  if (heap.unbound(specific)) return false;
  if (integer_cell(heap, general) && integer_cell(heap, specific)) {
    return heap.integer_value(general) == heap.integer_value(specific);
  }
  // 期待の -1 は前置の複合項。投げた整数とタグが違うので、数の符号だけ揃える。
  if (sign_of_number(heap, general) || sign_of_number(heap, specific)) {
    const auto left = signed_number(heap, general);
    const auto right = signed_number(heap, specific);
    if (left && right) return same_number(*left, *right);
  }
  if (general.tag() != specific.tag()) return false;
  if (general.tag() == Tag::Atom) return atom_text(heap, general) == atom_text(heap, specific);
  if (general.tag() == Tag::Float) return heap.float_value(general) == heap.float_value(specific);
  if (general.tag() == Tag::Lis) {
    return subsumes(heap, heap.list_head(general), heap.list_head(specific), bound) &&
           subsumes(heap, heap.list_tail(general), heap.list_tail(specific), bound);
  }
  if (general.tag() != Tag::Str || heap.functor_arity(general) != heap.functor_arity(specific)) return false;
  if (heap.atoms().name(heap.functor_atom(general)) != heap.atoms().name(heap.functor_atom(specific))) return false;
  const auto arity = heap.functor_arity(general);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!subsumes(heap, heap.arg(general, index), heap.arg(specific, index), bound)) return false;
  }
  return true;
}

}  // namespace

bool lgt_same_shape(const Heap& heap, Cell left, Cell right) { return same_shape(heap, left, right); }

bool lgt_subsumes(const Heap& heap, Cell general, Cell specific) {
  std::vector<std::pair<std::uint64_t, Cell>> bound;
  return subsumes(heap, general, specific, bound);
}

}  // namespace mosa
