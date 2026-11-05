#pragma once

// session_test と full_stop_test が REPL の出力を行ごとに比べる。

#include <cstddef>
#include <string_view>

// REPL の出力に、行全体が line の答えがあるか（行の頭のプロンプトは除く）。部分文字列で探すと、余分な部分が前に
// 付いた答えも通る（M26 の敵対的レビュー A4）。
inline bool answered(std::string_view out, std::string_view line) {
  std::size_t start = 0;
  while (start < out.size()) {
    std::size_t end = out.find('\n', start);
    if (end == std::string_view::npos) end = out.size();
    std::string_view row = out.substr(start, end - start);
    while (row.starts_with("?- ")) row.remove_prefix(3);
    if (row == line) return true;
    start = end + 1;
  }
  return false;
}
