// 判定器の =~= と、表明の数を double で読むところ。lgt_judge.cpp から分けた。

#include "lgt_priv.hpp"

#include "mosa/bigint.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace mosa {
namespace {

// Big は評価の float/1 と同じ正しい丸め（最近接偶数）で double にする。double の範囲の外は無限大にせず数でないと
// 扱い、表明は不成立になる。integer_value は Big を読めない（ADR 0025）。
std::optional<double> integer_double(const Heap& heap, Cell cell) {
  if (cell.tag() == Tag::Big) return to_double(heap.big_value(cell));
  return static_cast<double>(heap.integer_value(cell));
}

}  // namespace

std::optional<double> lgt_as_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (is_integer_tag(cell.tag())) return integer_double(heap, cell);
  if (cell.tag() == Tag::Float) return heap.float_value(cell);
  // 表明の -125.0 は前置のまま残る。=~= は算術なので、数の符号だけ畳む。
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "-" && name != "+") return std::nullopt;
  const Cell arg = heap.deref(heap.arg(cell, 0));
  std::optional<double> inner;
  if (is_integer_tag(arg.tag())) inner = integer_double(heap, arg);
  else if (arg.tag() == Tag::Float) inner = heap.float_value(arg);
  if (!inner || name == "+") return inner;
  return -*inner;
}

namespace {

// 出典は固定コミット 4decb64 の tools/lgtunit/lgtunit.lgt。Mosa は epsilon/1 の方言の一覧に無いので、
// else の枝の 0.000000000001 になる。
constexpr double lgtunit_epsilon = 0.000000000001;

// まず絶対誤差 100*epsilon を見る。0 に近い数はこちらでしか意味のある比較にならない。
// だめなら相対誤差（99.999% の精度）を見る。どちらも lgtunit と同じく厳密な < で比べる。
bool approx_number(double first, double second) {
  const double gap = std::fabs(first - second);
  if (gap < 100 * lgtunit_epsilon) return true;
  return gap < 0.00001 * std::max(std::fabs(first), std::fabs(second));
}

bool is_nil(const Heap& heap, Cell cell) {
  return cell.tag() == Tag::Atom && heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == "[]";
}

}  // namespace

// lgtunit の (=~=)/2 は数どうしか、同じ長さの数のリストどうしを要素ごとに比べる。
// 変数と数でないものは lgtunit では誤りを投げるが、表明としてはどちらも不成立なので偽を返す。
bool approx(const Heap& heap, Cell left, Cell right) {
  left = heap.deref(left);
  right = heap.deref(right);
  while (left.tag() == Tag::Lis && right.tag() == Tag::Lis) {
    if (!approx(heap, heap.list_head(left), heap.list_head(right))) return false;
    left = heap.deref(heap.list_tail(left));
    right = heap.deref(heap.list_tail(right));
  }
  if (is_nil(heap, left) || is_nil(heap, right)) return is_nil(heap, left) && is_nil(heap, right);
  const auto first = lgt_as_number(heap, left);
  const auto second = lgt_as_number(heap, right);
  if (!first || !second) return false;
  return approx_number(*first, *second);
}

}  // namespace mosa
