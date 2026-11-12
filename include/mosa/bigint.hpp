#pragma once

// ヒープの外の任意精度の整数（フェーズ10、ADR 0025）。算術の評価と字面の変換がこの型で計算し、ヒープの Big の箱とは
// from_words / words の 64 ビットの語で受け渡す。マシンにもヒープにも依存しない。
// 符号と絶対値で持つ。絶対値は 32 ビットの語（下位が先）で、64 ビットどうしの積を標準の型だけで書けるようにする。
// 実装: src/core/bigint.cpp（加減乗、比較、シフト、ビット演算）、bigint_div.cpp（商、gcd、累乗）、
// bigint_text.cpp（字面と double）。テスト: tests/cpp/bigint_test.cpp

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

// 乗算を Karatsuba に切り替える大きさ（短い方の被乗数の、32 ビットの語の数）。これより短ければ 2 乗の掛け算にする。
// 2026-11-12 に Apple M1 Max（clang -O3）で、同じ長さの乱数どうしの積 1 回の時間を測った（2 乗 / Karatsuba）:
// 32 語 0.87 / 1.18 µs、40 語 1.81 / 1.57 µs、48 語 2.27 / 2.02 µs、64 語 4.31 / 3.21 µs。
// 32 語と 40 語の間で逆転する。4096 語では閾値 32・40・48・64 が 3086・2778・2792・2791 µs（2 乗だけなら 25879 µs）。
inline constexpr std::size_t kKaratsubaWords = 40;

struct BigIntAccess;

class BigInt {
 public:
  BigInt() = default;  // 0
  explicit BigInt(std::int64_t value);
  // 絶対値の 64 ビットの語（下位が先）と符号から作る。上位の 0 の語は落とし、0 の符号は正にする。
  [[nodiscard]] static BigInt from_words(std::span<const std::uint64_t> magnitude, bool negative);
  [[nodiscard]] bool negative() const { return negative_; }
  [[nodiscard]] bool is_zero() const { return limbs_.empty(); }
  [[nodiscard]] std::optional<std::int64_t> to_int64() const;
  [[nodiscard]] std::vector<std::uint64_t> words() const;  // 絶対値。下位が先、上位の 0 なし、0 は空
  [[nodiscard]] std::uint64_t bit_length() const;          // 絶対値のビット数。0 は 0
  [[nodiscard]] BigInt operator-() const;
  friend bool operator==(const BigInt&, const BigInt&) = default;
  friend std::strong_ordering operator<=>(const BigInt& a, const BigInt& b);
  friend BigInt operator+(const BigInt& a, const BigInt& b);
  friend BigInt operator-(const BigInt& a, const BigInt& b);
  friend BigInt operator*(const BigInt& a, const BigInt& b);

 private:
  friend struct BigIntAccess;  // src/core/ の実装のファイルが語を直に読むための入口（bigint_limbs.hpp）

  // 上位の 0 の語は持たず、0 は空で正。この正規形があるので、等しさは欄どうしの比較で済む。
  std::vector<std::uint32_t> limbs_;
  bool negative_ = false;
};

struct BigQuotient {
  BigInt quotient;
  BigInt remainder;
};

[[nodiscard]] BigQuotient divide_truncate(const BigInt& a, const BigInt& b);  // 前提 b ≠ 0。余りは a の符号
[[nodiscard]] BigQuotient divide_floor(const BigInt& a, const BigInt& b);     // 前提 b ≠ 0。余りは b の符号
[[nodiscard]] BigInt shift_left(const BigInt& a, std::uint64_t bits);
[[nodiscard]] BigInt shift_right(const BigInt& a, std::uint64_t bits);  // 床に丸める（算術シフト）
[[nodiscard]] BigInt bit_and(const BigInt& a, const BigInt& b);         // 以下 4 つは 2 の補数の意味
[[nodiscard]] BigInt bit_or(const BigInt& a, const BigInt& b);
[[nodiscard]] BigInt bit_xor(const BigInt& a, const BigInt& b);
[[nodiscard]] BigInt bit_not(const BigInt& a);
[[nodiscard]] BigInt gcd(const BigInt& a, const BigInt& b);  // 非負
[[nodiscard]] BigInt power(const BigInt& base, std::uint64_t exponent);
[[nodiscard]] std::string to_text(const BigInt& value, unsigned radix = 10);  // 負なら '-' を先に。小文字。前提 2〜36
// 数字だけ（符号、'_'、接頭辞なし）。radix は 2・8・10・16。数字でない字があれば nullopt。
[[nodiscard]] std::optional<BigInt> parse_magnitude(std::string_view digits, unsigned radix);
[[nodiscard]] std::optional<double> to_double(const BigInt& value);  // 最近接偶数。範囲外は nullopt
[[nodiscard]] BigInt from_integral_double(double value);             // 前提: 有限で整数値
[[nodiscard]] std::uint64_t big_key(bool negative, std::span<const std::uint64_t> magnitude);  // src/core/fnv.hpp の式

}  // namespace mosa
