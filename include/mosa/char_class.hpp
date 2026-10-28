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
// 単純大文字写像。写像が無ければ符号位置をそのまま返す。
[[nodiscard]] char32_t simple_uppercase(char32_t code_point);
// 半角カタカナの互換分解。to_full なら半角から全角、そうでなければ逆。対が無ければそのまま。
[[nodiscard]] char32_t halfwidth_partner(char32_t code_point, bool to_full);
[[nodiscard]] std::string_view unicode_version();

}
