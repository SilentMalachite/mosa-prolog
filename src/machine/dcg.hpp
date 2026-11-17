#pragma once

// 文法規則の変換（ADR 0024）。ローダ（load.cpp）、phrase/2,3（builtins/phrase.cpp）、expand_term/2 の下の '$dcg_translate'/2（builtins/dcg_translate.cpp）が使う。

#include "internal.hpp"

#include <optional>

namespace mosa {

struct DcgOut {
  Cell term{};
  // 訳せなかったときの Formal。not_callable は type_error(callable, _) であること。phrase/2,3 は Culprit を
  // 渡された本体全体に替える（call/1 と同じく、7.8.3.3）。
  std::optional<Cell> formal;
  bool not_callable = false;
  // 本体の制御構造が自分に戻った（戻り辺は phrase(V, S0, S) に訳してある）。phrase/2,3 はこれで
  // resource_error(cyclic_term) を投げる（iso-notes 7.3.3、2026-11-17 の持ち主の決定 A）。文法規則は節を作るので、
  // assertz/1 の節（レビュー M7）と同じく見ない。
  bool cyclic = false;
};

// 項が -->/2 か。ローダと '$dcg_translate'/2 が同じ判定を使う。
[[nodiscard]] inline bool is_rule_arrow(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == 2 && heap.atoms().name(heap.functor_atom(cell)) == "-->";
}

// H --> B を H' :- B' に訳す。
[[nodiscard]] DcgOut dcg_rule(Impl& machine, Cell rule);
// 文法規則の本体 Body を、S0 から読み始めて S を残すゴールに訳す。
[[nodiscard]] DcgOut dcg_body(Impl& machine, Cell body, Cell s0, Cell s);

}  // namespace mosa
