#pragma once

// 文法規則の変換（ADR 0024）。ローダ（load.cpp）と phrase/2,3（builtins/phrase.cpp）が使う。

#include "internal.hpp"

#include <optional>

namespace mosa {

struct DcgOut {
  Cell term{};
  // 訳せなかったときの Formal。not_callable は type_error(callable, _) であること。phrase/2,3 は Culprit を
  // 渡された本体全体に替える（call/1 と同じく、7.8.3.3）。
  std::optional<Cell> formal;
  bool not_callable = false;
};

// H --> B を H' :- B' に訳す。
[[nodiscard]] DcgOut dcg_rule(Impl& machine, Cell rule);
// 文法規則の本体 Body を、S0 から読み始めて S を残すゴールに訳す。
[[nodiscard]] DcgOut dcg_body(Impl& machine, Cell body, Cell s0, Cell s);

}  // namespace mosa
