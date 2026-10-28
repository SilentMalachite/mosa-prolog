#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace mosa {

// arith.cpp の eval_known と arith_float.cpp の float_op が振り分ける関数子。is/2 は引数を評価する前に
// この表を引く（iso-notes 7.9.1 の決定 B）。関数子を足すときは両方を直す。
// 計算する側と揃っていることは tests/cpp/evaluable_test.cpp が確かめる。
inline constexpr std::array<std::string_view, 25> kUnaryEvaluable{
    "+",   "-",    "abs", "sign",  "msb",   "succ_or_zero", "\\",       "sqrt",
    "sin", "cos",  "tan", "asin",  "acos",  "atan",         "exp",      "log",
    "log2", "float", "float_integer_part", "float_fractional_part", "integer", "truncate",
    "floor", "ceiling", "round",
};

inline constexpr std::array<std::string_view, 22> kBinaryEvaluable{
    "+",  "-",  "*",  "/",   "//",  "rem",   "div",      "mod",        "^",   "**",  "min",
    "max", "gcd", ">>", "<<", "/\\", "\\/", "xor", "atan2", "copysign", "nexttoward", "log",
};

inline bool is_evaluable(std::string_view name, std::size_t arity) {
  if (arity == 1) return std::find(kUnaryEvaluable.begin(), kUnaryEvaluable.end(), name) != kUnaryEvaluable.end();
  if (arity == 2) return std::find(kBinaryEvaluable.begin(), kBinaryEvaluable.end(), name) != kBinaryEvaluable.end();
  return false;
}

}  // namespace mosa
