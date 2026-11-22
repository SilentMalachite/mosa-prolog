#pragma once

// manual_parse_test.cpp と manual_table_test.cpp が共に使う道具。2 つは 1 つの manual_parse_test の実行ファイルに
// 入るので、無名の名前空間に写しを 2 つ持つと、片方が使わない写しが -Werror で落ちる。

#include "mosa/manual.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace manual_parse_support {

inline std::string with_crlf(std::string_view text) {
  std::string out;
  for (const char letter : text) {
    if (letter == '\n') out += '\r';
    out += letter;
  }
  return out;
}

inline bool has_error(const mosa::ParsedManual& parsed, std::string_view prefix) {
  for (const std::string& error : parsed.errors) {
    if (error.starts_with(prefix)) return true;
  }
  return false;
}

inline bool same(const std::vector<mosa::PredicateIndicator>& entries,
                 const std::vector<mosa::PredicateIndicator>& wanted) {
  return entries == wanted;
}

}  // namespace manual_parse_support
