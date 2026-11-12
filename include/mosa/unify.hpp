#pragma once

// 出現検査はしない。失敗したときは、その呼び出しで束縛した REF をすべて戻す。
// 成功したトレイルには、選択点より前の REF だけを残す。新しいセルはヒープを戻すと消える。

#include "mosa/heap.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace mosa {

// 単一化で同じ浮動小数の項か。mosatest --roundtrip の照合（term_equal.cpp）と Logtalk の照合（lgt_shape.cpp）も使う。
// -0.0 と 0.0 は writeq でも copysign でも区別できるので別の項にする（iso-notes 7.2.2 の決定 A、レビュー L2）。
// NaN は前から自分とも単一化しない。== と compare/3 は order.cpp の標準順序で、そこでは NaN どうしは等しい。
[[nodiscard]] inline bool same_float_term(double left, double right) {
  return left == right && std::signbit(left) == std::signbit(right);
}

// 単一化と変種の照合で同じ Big か（2 つともタグが Big のとき）。正規形の規則（ADR 0025）で同じ値は同じ語の並びに
// なるので、値に直さずに符号と全語を比べる。ヒープが別でもよい（mosatest --roundtrip の照合）。
[[nodiscard]] bool same_big_term(const Heap& left_heap, Cell left, const Heap& right_heap, Cell right);

struct Trail {
  std::vector<std::uint64_t*> slots;
  // 選択点のヒープ水位。nullptr のときは、今回の呼び出し開始水位だけを境にする。
  const std::uint64_t* mark = nullptr;
};

[[nodiscard]] bool unify(Heap& heap, Trail& trail, std::size_t trail_mark, std::size_t heap_mark, Cell left,
                         Cell right);
// 成功したとき、この呼び出しで束縛した REF を bound の後ろに足す。unify_with_occurs_check/2 が束縛した値を調べる。
[[nodiscard]] bool unify(Heap& heap, Trail& trail, std::size_t trail_mark, std::size_t heap_mark, Cell left,
                         Cell right, std::vector<Cell>& bound);

}
