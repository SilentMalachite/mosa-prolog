#pragma once

#include <string_view>

// 浮動小数点数の綴りを double にする（リリース 0.1.0 Task 10b）。実装は src/platform/parse_double.cpp。
namespace mosa {

enum class DoubleKind { Value, OutOfRange, Invalid };

struct DoubleParse {
  DoubleKind kind = DoubleKind::Invalid;
  double value = 0.0;  // DoubleKind::Value のときだけ意味を持つ
};

// std::from_chars(first, last, double&) の三つの場合を返す。読んだ頭が範囲外（上位か下位の溢れ）なら読み残しが
// あっても OutOfRange、そうでなく読めないか全部を読まなければ Invalid。非正規化数は値として返す。
[[nodiscard]] DoubleParse parse_double(std::string_view text);

}  // namespace mosa
