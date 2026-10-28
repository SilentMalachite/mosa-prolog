#pragma once

// ISO 13211-1 6.5 の文字分類を、非 ASCII では Unicode の一般カテゴリへ広げる。
// ASCII は規格の表のまま固定し、生成表では決めない。

#include <optional>
#include <string_view>

namespace mosa {

enum class CharClass {
  Layout, Small, Capital, Underline, Digit, Graphic, Solo, Mark, Quote, Illegal
};

[[nodiscard]] CharClass classify(char32_t code_point);
[[nodiscard]] bool east_asian_wide(char32_t code_point);
// UnicodeData.txt の単純大文字化。写像の無い文字は自身を返す。1 文字が複数になる写像は扱わない。
[[nodiscard]] char32_t simple_upper(char32_t code_point);
[[nodiscard]] std::string_view unicode_version();
// 全角記号モード（SPEC 第4節）で半角に読む 7 字。。と「」は前後を見るので字句解析器が扱う。
[[nodiscard]] std::optional<char32_t> halfwidth_punctuation(char32_t code_point);

}
