#pragma once

// 差分テスト（計画 Task 11）が答えとエラー項を比べるための字面。write_term は項の入れ子の深さだけ C++ の再帰を使い、
// tests/pl/types.pl の ground_deep（f(...) を 30000 段）は 8 MB の既定のスタックで書けない（ctest は子のスタックの
// 上限を上げるので通るが、シェルから流すと落ち、Windows の 1 MB では確実に落ちる）。両方の側が同じ字面になればよいので、
// 明示のスタックで辿り、演算子を使わない関数表記で書く。葉（アトム、数）だけ write_term で書き、引用を writer に合わせる。
// 名前の無い変数は最初の出現の順の _G<n>、再び入った開いた構造は writer と同じく _ にする。テストランナーの外には出さない。

#include "mosa/heap.hpp"
#include "mosa/operator_table.hpp"

#include <string>

namespace mosa {

[[nodiscard]] std::string answer_text(const Heap& heap, OperatorView operators, Cell term);

}  // namespace mosa
