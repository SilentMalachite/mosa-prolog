// 判定器の =~= と、表明の数を double で読むところ。lgt_judge.cpp から分けた。

#include "lgt_priv.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace mosa {

std::optional<double> lgt_as_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) return static_cast<double>(heap.integer_value(cell));
  if (cell.tag() == Tag::Float) return heap.float_value(cell);
  // 表明の -125.0 は前置のまま残る。=~= は算術なので、数の符号だけ畳む。
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "-" && name != "+") return std::nullopt;
  const Cell arg = heap.deref(heap.arg(cell, 0));
  std::optional<double> inner;
  if (arg.tag() == Tag::Int || arg.tag() == Tag::IntBox) inner = static_cast<double>(heap.integer_value(arg));
  else if (arg.tag() == Tag::Float) inner = heap.float_value(arg);
  else return std::nullopt;
  if (name == "+") return inner;
  return -*inner;
}

// epsilon/0 と同じ定数。ここで query すると、本体の選択点が消える。
bool approx(const Heap& heap, Cell left, Cell right) {
  const auto first = lgt_as_number(heap, left);
  const auto second = lgt_as_number(heap, right);
  if (!first || !second) return false;
  const double epsilon = std::numeric_limits<double>::epsilon();
  const double scale = std::max(std::fabs(*first), std::fabs(*second));
  return std::fabs(*first - *second) <= scale * epsilon;
}

}  // namespace mosa
