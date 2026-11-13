// BigInt の加減乗、比較、シフト、2 の補数の意味のビット演算、索引の鍵（include/mosa/bigint.hpp）。
// 商、gcd、累乗は bigint_div.cpp、字面と double は bigint_text.cpp。テスト: tests/cpp/bigint_test.cpp

#include "bigint_limbs.hpp"
#include "fnv.hpp"

#include <algorithm>
#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// out += x。繰り上がりは out の中で止まることを前提にする（呼ぶ側が結果の語数を確保する）。
void add_into(std::span<std::uint32_t> out, LimbSpan x) {
  std::uint64_t carry = 0;
  std::size_t i = 0;
  for (; i < x.size(); ++i) {
    const std::uint64_t sum = std::uint64_t{out[i]} + x[i] + carry;
    out[i] = static_cast<std::uint32_t>(sum);
    carry = sum >> kLimbBits;
  }
  for (; carry != 0 && i < out.size(); ++i) {
    const std::uint64_t sum = std::uint64_t{out[i]} + carry;
    out[i] = static_cast<std::uint32_t>(sum);
    carry = sum >> kLimbBits;
  }
}

// out -= x（前提 out ≥ x）。64 ビットで引けば、負になったときだけ最上位ビットが立つので、それを借りとする。
void subtract_into(std::span<std::uint32_t> out, LimbSpan x) {
  std::uint64_t borrow = 0;
  std::size_t i = 0;
  for (; i < x.size(); ++i) {
    const std::uint64_t diff = std::uint64_t{out[i]} - x[i] - borrow;
    out[i] = static_cast<std::uint32_t>(diff);
    borrow = diff >> 63;
  }
  for (; borrow != 0 && i < out.size(); ++i) {
    const std::uint64_t diff = std::uint64_t{out[i]} - borrow;
    out[i] = static_cast<std::uint32_t>(diff);
    borrow = diff >> 63;
  }
}

// 32 ビットどうしの積に 2 つの 32 ビットを足しても 2^64 − 1 を越えないので、繰り上がりは 1 語で足りる。
Limbs schoolbook(LimbSpan a, LimbSpan b) {
  Limbs out(a.size() + b.size(), 0);
  for (std::size_t i = 0; i < a.size(); ++i) {
    const std::uint64_t digit = a[i];
    if (digit == 0) continue;
    std::uint64_t carry = 0;
    for (std::size_t j = 0; j < b.size(); ++j) {
      const std::uint64_t t = digit * b[j] + out[i + j] + carry;
      out[i + j] = static_cast<std::uint32_t>(t);
      carry = t >> kLimbBits;
    }
    out[i + b.size()] = static_cast<std::uint32_t>(carry);
  }
  return out;
}

Limbs multiply_trimmed(LimbSpan a, LimbSpan b);

// 長い方を短い方の長さで切って掛け、ずらして足す。釣り合わない積でも、各切れが釣り合った Karatsuba に乗る。
Limbs multiply_unbalanced(LimbSpan longer, LimbSpan shorter) {
  Limbs out(longer.size() + shorter.size(), 0);
  for (std::size_t at = 0; at < longer.size(); at += shorter.size()) {
    const LimbSpan piece = trimmed(longer.subspan(at, std::min(shorter.size(), longer.size() - at)));
    const Limbs product = multiply_trimmed(piece, shorter);
    add_into(std::span(out).subspan(at), product);
  }
  return out;
}

// a = a1·B^h + a0、b = b1·B^h + b0 と分け、(a0+a1)(b0+b1) − a0b0 − a1b1 で中の項を作る。積 3 回で済む。
// 前提 a.size() ≥ b.size() > a.size() / 2。このとき b1 は空にならない。
Limbs karatsuba(LimbSpan a, LimbSpan b) {
  const std::size_t half = a.size() / 2;
  const LimbSpan a0 = trimmed(a.first(half));
  const LimbSpan b0 = trimmed(b.first(half));
  const LimbSpan a1 = a.subspan(half);
  const LimbSpan b1 = b.subspan(half);
  const Limbs low = multiply_trimmed(a0, b0);
  const Limbs high = multiply_trimmed(a1, b1);
  Limbs middle = multiply_trimmed(add_magnitude(a0, a1), add_magnitude(b0, b1));
  subtract_into(middle, low);
  subtract_into(middle, high);
  Limbs out(a.size() + b.size(), 0);
  add_into(out, low);
  add_into(std::span(out).subspan(half), trimmed(middle));
  add_into(std::span(out).subspan(2 * half), high);
  return out;
}

// 結果はいつも正規化して返す。karatsuba の引き算は、引く側と引かれる側が正規化されていることに頼る。
Limbs multiply_trimmed(LimbSpan a, LimbSpan b) {
  if (a.size() < b.size()) std::swap(a, b);
  Limbs out;
  if (b.empty()) return out;
  if (b.size() < kKaratsubaWords) {
    out = schoolbook(a, b);
  } else if (2 * b.size() <= a.size()) {
    out = multiply_unbalanced(a, b);
  } else {
    out = karatsuba(a, b);
  }
  trim(out);
  return out;
}

// 符号が同じなら絶対値を足す。違えば大きい方から小さい方を引き、大きい方の符号を付ける。
BigInt add_signed(LimbSpan a, bool a_negative, LimbSpan b, bool b_negative) {
  if (a_negative == b_negative) return BigIntAccess::make(add_magnitude(a, b), a_negative);
  if (compare_magnitude(a, b) >= 0) return BigIntAccess::make(subtract_magnitude(a, b), a_negative);
  return BigIntAccess::make(subtract_magnitude(b, a), b_negative);
}

Limbs shift_left_magnitude(LimbSpan a, std::uint64_t bits) {
  if (a.empty()) return {};
  const auto words = static_cast<std::size_t>(bits / kLimbBits);
  const auto offset = static_cast<unsigned>(bits % kLimbBits);
  Limbs out(words + a.size() + 1, 0);
  for (std::size_t i = 0; i < a.size(); ++i) {
    const std::uint64_t moved = std::uint64_t{a[i]} << offset;
    out[words + i] |= static_cast<std::uint32_t>(moved);
    out[words + i + 1] = static_cast<std::uint32_t>(moved >> kLimbBits);
  }
  trim(out);
  return out;
}

// ~x + 1 をその場で。語数を固定した 2 の補数として扱うので、最上位からの繰り上がりは捨てる。
void negate_twos(Limbs& limbs) {
  std::uint64_t carry = 1;
  for (auto& limb : limbs) {
    const std::uint64_t t = std::uint64_t{static_cast<std::uint32_t>(~limb)} + carry;
    limb = static_cast<std::uint32_t>(t);
    carry = t >> kLimbBits;
  }
}

// 両方の絶対値より 1 語長くとるので、最上位の語の最上位ビットがいつも符号のビットになる。
Limbs to_twos(const BigInt& value, std::size_t size) {
  const Limbs& limbs = BigIntAccess::limbs(value);
  Limbs out(size, 0);
  std::copy(limbs.begin(), limbs.end(), out.begin());
  if (value.negative()) negate_twos(out);
  return out;
}

BigInt from_twos(Limbs limbs) {
  const bool negative = !limbs.empty() && (limbs.back() >> (kLimbBits - 1)) != 0;
  if (negative) negate_twos(limbs);
  return BigIntAccess::make(std::move(limbs), negative);
}

template <typename Op>
BigInt bitwise(const BigInt& a, const BigInt& b, Op op) {
  const std::size_t size = std::max(BigIntAccess::limbs(a).size(), BigIntAccess::limbs(b).size()) + 1;
  Limbs x = to_twos(a, size);
  const Limbs y = to_twos(b, size);
  for (std::size_t i = 0; i < size; ++i) x[i] = static_cast<std::uint32_t>(op(x[i], y[i]));
  return from_twos(std::move(x));
}

}  // namespace

// 引数の名前を、メンバ関数（BigIntAccess::limbs と、from_words では BigInt::negative）とずらす。MSVC の /W4 は
// メンバを隠す名前を警告する（C4458）。
BigInt BigIntAccess::make(Limbs magnitude, bool is_negative) {
  trim(magnitude);
  BigInt out;
  out.negative_ = is_negative && !magnitude.empty();
  out.limbs_ = std::move(magnitude);
  return out;
}

void trim(Limbs& limbs) {
  while (!limbs.empty() && limbs.back() == 0) limbs.pop_back();
}

LimbSpan trimmed(LimbSpan limbs) {
  std::size_t size = limbs.size();
  while (size > 0 && limbs[size - 1] == 0) --size;
  return limbs.first(size);
}

std::strong_ordering compare_magnitude(LimbSpan a, LimbSpan b) {
  if (a.size() != b.size()) return a.size() <=> b.size();
  for (std::size_t i = a.size(); i-- > 0;) {
    if (a[i] != b[i]) return a[i] <=> b[i];
  }
  return std::strong_ordering::equal;
}

Limbs add_magnitude(LimbSpan a, LimbSpan b) {
  if (a.size() < b.size()) std::swap(a, b);
  Limbs out(a.size() + 1, 0);
  std::copy(a.begin(), a.end(), out.begin());
  add_into(out, b);
  trim(out);
  return out;
}

Limbs subtract_magnitude(LimbSpan a, LimbSpan b) {
  Limbs out(a.begin(), a.end());
  subtract_into(out, b);
  trim(out);
  return out;
}

// 32 ビットどうしの積に 32 ビットを足しても 64 ビットに収まるので、繰り上がりは 1 語で足りる。
void multiply_add_small(Limbs& limbs, std::uint32_t factor, std::uint32_t addend) {
  std::uint64_t carry = addend;
  for (auto& limb : limbs) {
    const std::uint64_t t = std::uint64_t{limb} * factor + carry;
    limb = static_cast<std::uint32_t>(t);
    carry = t >> kLimbBits;
  }
  if (carry != 0) limbs.push_back(static_cast<std::uint32_t>(carry));
  trim(limbs);
}

ShiftedLimbs shift_right_magnitude(LimbSpan a, std::uint64_t bits) {
  // 語数より大きいシフトは、語の位置を計算する前に返す。bits は 2^64 近くまで来うる。
  if (bits / kLimbBits >= a.size()) return {{}, !a.empty()};
  const auto words = static_cast<std::size_t>(bits / kLimbBits);
  const auto offset = static_cast<unsigned>(bits % kLimbBits);
  const LimbSpan dropped = a.first(words);
  bool lost = std::any_of(dropped.begin(), dropped.end(), [](std::uint32_t limb) { return limb != 0; });
  if (offset != 0) lost = lost || (a[words] & ((std::uint32_t{1} << offset) - 1U)) != 0;
  Limbs out(a.size() - words, 0);
  for (std::size_t i = 0; i < out.size(); ++i) {
    const std::uint64_t above = words + i + 1 < a.size() ? std::uint64_t{a[words + i + 1]} << kLimbBits : 0;
    out[i] = static_cast<std::uint32_t>((std::uint64_t{a[words + i]} | above) >> offset);
  }
  trim(out);
  return {std::move(out), lost};
}

BigInt::BigInt(std::int64_t value) : negative_(value < 0) {
  // INT64_MIN の絶対値は int64 に入らないので、符号なしの 0 から引いて作る。
  const std::uint64_t magnitude =
      value < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
  if (magnitude != 0) limbs_.push_back(static_cast<std::uint32_t>(magnitude));
  if ((magnitude >> kLimbBits) != 0) limbs_.push_back(static_cast<std::uint32_t>(magnitude >> kLimbBits));
}

BigInt BigInt::from_words(std::span<const std::uint64_t> magnitude, bool is_negative) {
  Limbs limbs;
  limbs.reserve(magnitude.size() * 2);
  for (const std::uint64_t word : magnitude) {
    limbs.push_back(static_cast<std::uint32_t>(word));
    limbs.push_back(static_cast<std::uint32_t>(word >> kLimbBits));
  }
  return BigIntAccess::make(std::move(limbs), is_negative);
}

std::optional<std::int64_t> BigInt::to_int64() const {
  if (limbs_.size() > 2) return std::nullopt;
  std::uint64_t magnitude = 0;
  for (std::size_t i = limbs_.size(); i-- > 0;) magnitude = (magnitude << kLimbBits) | limbs_[i];
  constexpr std::uint64_t kLimit = std::uint64_t{1} << 63;
  if (!negative_) {
    if (magnitude >= kLimit) return std::nullopt;
    return static_cast<std::int64_t>(magnitude);
  }
  if (magnitude > kLimit) return std::nullopt;
  // 2^64 − magnitude を int64 に写せば −magnitude になる。2^63 も −2^63 を経ずに INT64_MIN になる。
  return static_cast<std::int64_t>(std::uint64_t{0} - magnitude);
}

std::vector<std::uint64_t> BigInt::words() const {
  std::vector<std::uint64_t> out((limbs_.size() + 1) / 2, 0);
  for (std::size_t i = 0; i < limbs_.size(); ++i) {
    out[i / 2] |= std::uint64_t{limbs_[i]} << (kLimbBits * (i % 2));
  }
  return out;
}

std::uint64_t BigInt::bit_length() const {
  if (limbs_.empty()) return 0;
  return static_cast<std::uint64_t>(limbs_.size() - 1) * kLimbBits +
         static_cast<std::uint64_t>(std::bit_width(limbs_.back()));
}

BigInt BigInt::operator-() const {
  BigInt out = *this;
  out.negative_ = !negative_ && !limbs_.empty();
  return out;
}

std::strong_ordering operator<=>(const BigInt& a, const BigInt& b) {
  if (a.negative_ != b.negative_) return a.negative_ ? std::strong_ordering::less : std::strong_ordering::greater;
  const std::strong_ordering order = compare_magnitude(a.limbs_, b.limbs_);
  return a.negative_ ? 0 <=> order : order;
}

BigInt operator+(const BigInt& a, const BigInt& b) { return add_signed(a.limbs_, a.negative_, b.limbs_, b.negative_); }

BigInt operator-(const BigInt& a, const BigInt& b) {
  return add_signed(a.limbs_, a.negative_, b.limbs_, !b.negative_);
}

BigInt operator*(const BigInt& a, const BigInt& b) {
  return BigIntAccess::make(multiply_trimmed(a.limbs_, b.limbs_), a.negative_ != b.negative_);
}

BigInt shift_left(const BigInt& a, std::uint64_t bits) {
  return BigIntAccess::make(shift_left_magnitude(BigIntAccess::limbs(a), bits), a.negative());
}

BigInt shift_right(const BigInt& a, std::uint64_t bits) {
  ShiftedLimbs shifted = shift_right_magnitude(BigIntAccess::limbs(a), bits);
  // 負の数は床へ丸めるので、1 のビットを落としたなら絶対値を 1 増やす（-5 >> 1 は -3）。
  if (a.negative() && shifted.lost) multiply_add_small(shifted.limbs, 1, 1);
  return BigIntAccess::make(std::move(shifted.limbs), a.negative());
}

BigInt bit_and(const BigInt& a, const BigInt& b) { return bitwise(a, b, std::bit_and<>()); }

BigInt bit_or(const BigInt& a, const BigInt& b) { return bitwise(a, b, std::bit_or<>()); }

BigInt bit_xor(const BigInt& a, const BigInt& b) { return bitwise(a, b, std::bit_xor<>()); }

// 2 の補数では ~x = −x − 1 なので、語の並びを広げずに済む。
BigInt bit_not(const BigInt& a) { return -a - BigInt(1); }

std::uint64_t big_key(bool negative, std::span<const std::uint64_t> magnitude) {
  // mosac が訳したコードと読み込み器が、どの OS でも同じ鍵を出すよう、語はシフトで下位のバイトから入れる
  // （src/machine/native.cpp の指紋と同じ順）。上位の 0 の語は無い前提なので、語数は入れなくても値が区別できる。
  std::uint64_t hash = fnv1a_byte(kFnvBasis, static_cast<std::uint8_t>(negative ? 1U : 0U));
  for (const std::uint64_t word : magnitude) {
    for (unsigned shift = 0; shift < 64; shift += 8) hash = fnv1a_byte(hash, static_cast<std::uint8_t>(word >> shift));
  }
  return hash;
}

}  // namespace mosa
