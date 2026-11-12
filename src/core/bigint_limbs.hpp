#pragma once

// BigInt の実装のファイル（bigint.cpp、bigint_div.cpp、bigint_text.cpp）が共有する、絶対値の語の並びの道具。
// 語の並びを扱う道具を公開の見出し include/mosa/bigint.hpp に出さないため、ここに置く。
// 語の並びはどれも下位が先。引数の LimbSpan は上位の 0 の語を持たない（trimmed の形）ことを前提にする。
// テスト: tests/cpp/bigint_test.cpp

#include "mosa/bigint.hpp"

#include <compare>
#include <cstdint>
#include <span>
#include <vector>

namespace mosa {

using Limbs = std::vector<std::uint32_t>;
using LimbSpan = std::span<const std::uint32_t>;

inline constexpr unsigned kLimbBits = 32;
inline constexpr std::uint64_t kLimbMask = 0xFFFF'FFFFULL;

struct BigIntAccess {
  [[nodiscard]] static const Limbs& limbs(const BigInt& value) { return value.limbs_; }
  // どの演算の結果もここで包む。ここで正規化するので、上位の 0 の語と負の 0 が外へ出ない。
  [[nodiscard]] static BigInt make(Limbs magnitude, bool is_negative);
};

struct ShiftedLimbs {
  Limbs limbs;
  bool lost = false;  // 右へ落としたビットに 1 があったか
};

void trim(Limbs& limbs);
[[nodiscard]] LimbSpan trimmed(LimbSpan limbs);
[[nodiscard]] std::strong_ordering compare_magnitude(LimbSpan a, LimbSpan b);
[[nodiscard]] Limbs add_magnitude(LimbSpan a, LimbSpan b);
[[nodiscard]] Limbs subtract_magnitude(LimbSpan a, LimbSpan b);  // 前提 a ≥ b
[[nodiscard]] Limbs multiply_magnitude(LimbSpan a, LimbSpan b);
void multiply_add_small(Limbs& limbs, std::uint32_t factor, std::uint32_t addend);
[[nodiscard]] ShiftedLimbs shift_right_magnitude(LimbSpan a, std::uint64_t bits);
// 商をその場に残し（正規化して）、余りを返す。前提 divisor ≠ 0。
[[nodiscard]] std::uint32_t divide_small(Limbs& limbs, std::uint32_t divisor);

}  // namespace mosa
