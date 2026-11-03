#pragma once

// writer.cpp だけが使う字面の判断。項を知らないので、writer の外の字句の判断と混ざらないよう名前空間を分ける。

#include <optional>
#include <string>
#include <string_view>

namespace mosa::spelling {

// UTF-8 の最初と最後の 1 字。壊れた字面なら nullopt。
[[nodiscard]] std::optional<char32_t> first_cp(std::string_view text);
[[nodiscard]] std::optional<char32_t> final_cp(std::string_view text);
// 英数字の名前を続ける字（6.4.2 の alphanumeric char と SPEC 第4節の Mark）。
[[nodiscard]] bool continues_name(char32_t code);
// 引用しなくてもその原子として読み戻せるか（7.10.5 h）。fullwidth は旗 japanese_punctuation の値。
[[nodiscard]] bool bare_atom(std::string_view name, bool fullwidth);
[[nodiscard]] std::string quote_atom(std::string_view text);
[[nodiscard]] std::string format_float(double value);

}  // namespace mosa::spelling
