#pragma once

// ISO 13211-1 6.5 の文字分類を、非 ASCII では Unicode の一般カテゴリへ広げる。
// ASCII は規格の表のまま固定し、生成表では決めない。

#include <string_view>

namespace mosa {

enum class CharClass {
  Layout, Small, Capital, Underline, Digit, Graphic, Solo, Mark, Quote, Illegal
};

[[nodiscard]] CharClass classify(char32_t code_point);
[[nodiscard]] bool east_asian_wide(char32_t code_point);
[[nodiscard]] std::string_view unicode_version();

}
