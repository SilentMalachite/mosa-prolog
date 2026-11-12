// BigInt と字面（2〜36 進）、BigInt と double の変換（include/mosa/bigint.hpp）。テスト: tests/cpp/bigint_test.cpp

#include "bigint_limbs.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mosa {
namespace {

constexpr std::string_view kDigits = "0123456789abcdefghijklmnopqrstuvwxyz";

// radix の何乗までが 1 語に入るか。1 回の短い割り算（掛け算）で、その桁数をまとめて取り出す（入れる）。
// 10 進なら 10^9 ずつになる。語が 32 ビットなので、10^19 では 1 語に入らない。
struct Chunk {
  std::uint32_t scale = 1;
  unsigned digits = 0;
};

Chunk chunk_for(unsigned radix) {
  std::uint64_t scale = 1;
  unsigned digits = 0;
  while (scale * radix <= kLimbMask) {
    scale *= radix;
    ++digits;
  }
  return {static_cast<std::uint32_t>(scale), digits};
}

std::optional<unsigned> digit_value(char unit) {
  if (unit >= '0' && unit <= '9') return static_cast<unsigned>(unit - '0');
  if (unit >= 'a' && unit <= 'z') return static_cast<unsigned>(unit - 'a') + 10U;
  if (unit >= 'A' && unit <= 'Z') return static_cast<unsigned>(unit - 'A') + 10U;
  return std::nullopt;
}

// 64 ビットに収まる語の並び（2 語以下）を 1 つの値にする。
std::uint64_t join_limbs(LimbSpan limbs) {
  std::uint64_t value = 0;
  for (std::size_t i = limbs.size(); i-- > 0;) value = (value << kLimbBits) | limbs[i];
  return value;
}

}  // namespace

// 下の桁から 1 語ぶんずつ割り出す 2 乗のアルゴリズム。最後の切れのほかは、上の 0 の桁も書く。
std::string to_text(const BigInt& value, unsigned radix) {
  if (value.is_zero()) return "0";
  const Chunk chunk = chunk_for(radix);
  Limbs rest = BigIntAccess::limbs(value);
  std::string reversed;
  while (!rest.empty()) {
    std::uint32_t piece = divide_small(rest, chunk.scale);
    for (unsigned k = 0; k < chunk.digits && (piece != 0 || !rest.empty()); ++k) {
      reversed.push_back(kDigits[piece % radix]);
      piece /= radix;
    }
  }
  if (value.negative()) reversed.push_back('-');
  return std::string(reversed.rbegin(), reversed.rend());
}

// 呼ぶ側（字句解析）は 2・8・10・16 だけを渡す。仕組みは 2〜36 のどれでも同じなので、その外だけを断る。
std::optional<BigInt> parse_magnitude(std::string_view digits, unsigned radix) {
  if (digits.empty() || radix < 2 || radix > kDigits.size()) return std::nullopt;
  const Chunk chunk = chunk_for(radix);
  Limbs limbs;
  std::size_t at = 0;
  while (at < digits.size()) {
    std::uint32_t piece = 0;
    std::uint32_t scale = 1;
    for (unsigned k = 0; k < chunk.digits && at < digits.size(); ++k, ++at) {
      const std::optional<unsigned> digit = digit_value(digits[at]);
      if (!digit || *digit >= radix) return std::nullopt;
      piece = piece * radix + *digit;
      scale *= radix;
    }
    multiply_add_small(limbs, scale, piece);
  }
  return BigIntAccess::make(std::move(limbs), false);
}

// 最上位の 1 から 64 ビットを取り、53 ビットの仮数、その下の 11 ビット、さらに下の sticky で最近接偶数に丸める。
// 丸めは整数のまま行う。64 ビットを double に変えてから丸めると、2 回丸めることになる。
std::optional<double> to_double(const BigInt& value) {
  const std::uint64_t length = value.bit_length();
  if (length == 0) return 0.0;
  const std::uint64_t drop = length > 64 ? length - 64 : 0;
  const ShiftedLimbs top = shift_right_magnitude(BigIntAccess::limbs(value), drop);
  const std::uint64_t bits = join_limbs(top.limbs) << (64 - (length - drop));
  std::uint64_t mantissa = bits >> 11;
  const std::uint64_t below = bits & 0x7FFU;
  constexpr std::uint64_t kHalf = 0x400U;
  if (below > kHalf || (below == kHalf && (top.lost || (mantissa & 1U) != 0))) ++mantissa;
  auto scale = static_cast<std::int64_t>(length) - 53;
  if (mantissa == (std::uint64_t{1} << 53)) {
    mantissa >>= 1;
    ++scale;
  }
  // 仮数は [2^52, 2^53) にあるので、値は 2^(52 + scale) 以上。2^1024 に届けば double の外になる。
  if (scale + 52 >= 1024) return std::nullopt;
  const double magnitude = std::ldexp(static_cast<double>(mantissa), static_cast<int>(scale));
  return value.negative() ? -magnitude : magnitude;
}

// 仮数を 53 ビットの整数に取り出せば、値は仮数 × 2^(指数 − 53) で正確に表せる。整数値なので右シフトは 0 だけを落とす。
BigInt from_integral_double(double value) {
  if (value == 0.0) return BigInt();
  int exponent = 0;
  const double fraction = std::frexp(std::fabs(value), &exponent);
  const BigInt mantissa(static_cast<std::int64_t>(std::ldexp(fraction, 53)));
  const int scale = exponent - 53;
  const BigInt magnitude = scale >= 0 ? shift_left(mantissa, static_cast<std::uint64_t>(scale))
                                      : shift_right(mantissa, static_cast<std::uint64_t>(-scale));
  return value < 0 ? -magnitude : magnitude;
}

}  // namespace mosa
