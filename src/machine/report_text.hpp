#pragma once

// 報告書を書く側（diff_report.cpp、bench_report.cpp、iso_suite.cpp、diff_tool.cpp）が共に使う字面。テストランナーの
// 外には出さない。テスト: tests/cpp/agreement_json_test.cpp、tests/cpp/bench_report_test.cpp、
// tests/cpp/conformance_badge_test.cpp

#include "mosa/path_text.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace mosa {

// JSON の文字列（RFC 8259 の 7 節）。" と \ のほか U+0000..U+001F も逃がす。前は " と \ だけを逃がし、タブや改行の
// 入ったディレクトリの名前で JSON が読めなくなった（レビュー L-39）。
[[nodiscard]] inline std::string json_quote(std::string_view text) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string out = "\"";
  for (const char unit : text) {
    const auto byte = static_cast<unsigned char>(unit);
    if (unit == '"' || unit == '\\') {
      out.push_back('\\');
      out.push_back(unit);
    } else if (unit == '\n') {
      out += "\\n";
    } else if (unit == '\t') {
      out += "\\t";
    } else if (unit == '\r') {
      out += "\\r";
    } else if (byte < 0x20U) {
      out += "\\u00";
      out.push_back(digits[byte >> 4U]);
      out.push_back(digits[byte & 0x0FU]);
    } else {
      out.push_back(unit);
    }
  }
  return out + "\"";
}

// 拡張子が lower（. 付きの小文字）か。ASCII の大小は区別しない。macOS と Windows の既定のファイルシステムでは
// X.JSON と X.json が同じファイルになる。
[[nodiscard]] inline bool extension_is(const std::filesystem::path& path, std::string_view lower) {
  std::string text = utf8_from_path(path.extension());
  for (char& unit : text) {
    if (unit >= 'A' && unit <= 'Z') unit = static_cast<char>(unit - 'A' + 'a');
  }
  return text == lower;
}

}  // namespace mosa
