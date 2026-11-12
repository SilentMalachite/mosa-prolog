// BigInt の商と余り（切り捨てと床）、gcd、累乗（include/mosa/bigint.hpp）。
// 複数の語で割るときは Knuth の Algorithm D（TAOCP 第 2 巻 4.3.1）。テスト: tests/cpp/bigint_test.cpp

#include "bigint_limbs.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct MagnitudeQuotient {
  Limbs quotient;
  Limbs remainder;
};

// size 語へ左に shift ビットずらして写す。shift は除数の最上位の語の上の 0 の数なので、除数ははみ出さない。
Limbs normalize(LimbSpan limbs, unsigned shift, std::size_t size) {
  Limbs out(size, 0);
  for (std::size_t i = 0; i < limbs.size(); ++i) {
    const std::uint64_t moved = std::uint64_t{limbs[i]} << shift;
    out[i] |= static_cast<std::uint32_t>(moved);
    if (i + 1 < size) out[i + 1] = static_cast<std::uint32_t>(moved >> kLimbBits);
  }
  return out;
}

// D3: 上の 2 語を除数の最上位の語で割って商の 1 語を見積もり、除数の上から 2 語目で直す。
// 除数を正規化してあるので、直したあとの見積もりは真の値か、1 大きいだけになる（Knuth の定理 B）。
std::uint64_t estimate_digit(const Limbs& un, const Limbs& vn, std::size_t j) {
  const std::size_t n = vn.size();
  const std::uint64_t top = (std::uint64_t{un[j + n]} << kLimbBits) | un[j + n - 1];
  std::uint64_t qhat = top / vn[n - 1];
  std::uint64_t rhat = top % vn[n - 1];
  // qhat が 1 語を越える間は掛け算を評価しない（左から順に評価されるので、積が 64 ビットからあふれない）。
  while (qhat > kLimbMask || qhat * vn[n - 2] > ((rhat << kLimbBits) | un[j + n - 2])) {
    --qhat;
    rhat += vn[n - 1];
    if (rhat > kLimbMask) break;
  }
  return qhat;
}

// D4〜D6: un[j..j+n] から qhat·vn を引く。引きすぎて負になったら vn を 1 回足し戻し、商を 1 減らす。
// 借りは符号なしの 64 ビットで引いた結果の最上位ビットで見る。積と借りを符号付きで扱うと、あふれる組がある。
std::uint32_t subtract_multiple(Limbs& un, const Limbs& vn, std::size_t j, std::uint64_t qhat) {
  const std::size_t n = vn.size();
  std::uint64_t carry = 0;
  std::uint64_t borrow = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint64_t product = qhat * vn[i] + carry;
    carry = product >> kLimbBits;
    const std::uint64_t diff = std::uint64_t{un[i + j]} - (product & kLimbMask) - borrow;
    un[i + j] = static_cast<std::uint32_t>(diff);
    borrow = diff >> 63;
  }
  const std::uint64_t top = std::uint64_t{un[j + n]} - carry - borrow;
  un[j + n] = static_cast<std::uint32_t>(top);
  if ((top >> 63) == 0) return static_cast<std::uint32_t>(qhat);
  std::uint64_t sum_carry = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint64_t sum = std::uint64_t{un[i + j]} + vn[i] + sum_carry;
    un[i + j] = static_cast<std::uint32_t>(sum);
    sum_carry = sum >> kLimbBits;
  }
  // 最上位の繰り上がりは、引いたときの借りと打ち消し合うので捨てる（Knuth の D6）。
  un[j + n] = static_cast<std::uint32_t>(un[j + n] + sum_carry);
  return static_cast<std::uint32_t>(qhat - 1);
}

// 前提 u ≥ v、v は 2 語以上。どちらも正規化済み。
MagnitudeQuotient divide_long(LimbSpan u, LimbSpan v) {
  const std::size_t n = v.size();
  const std::size_t m = u.size() - n;
  const auto shift = static_cast<unsigned>(std::countl_zero(v.back()));
  const Limbs vn = normalize(v, shift, n);
  Limbs un = normalize(u, shift, u.size() + 1);
  Limbs quotient(m + 1, 0);
  for (std::size_t j = m + 1; j-- > 0;) quotient[j] = subtract_multiple(un, vn, j, estimate_digit(un, vn, j));
  // 余りは un の下の n 語にあり、un[n] は 0。正規化のぶんを右へ戻す。
  Limbs remainder(n, 0);
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint64_t pair = std::uint64_t{un[i]} | (std::uint64_t{un[i + 1]} << kLimbBits);
    remainder[i] = static_cast<std::uint32_t>(pair >> shift);
  }
  trim(quotient);
  trim(remainder);
  return {std::move(quotient), std::move(remainder)};
}

MagnitudeQuotient divide_magnitude(LimbSpan u, LimbSpan v) {
  if (compare_magnitude(u, v) < 0) return {{}, Limbs(u.begin(), u.end())};
  if (v.size() == 1) {
    Limbs quotient(u.begin(), u.end());
    const std::uint32_t rest = divide_small(quotient, v[0]);
    return {std::move(quotient), rest == 0 ? Limbs{} : Limbs{rest}};
  }
  return divide_long(u, v);
}

}  // namespace

std::uint32_t divide_small(Limbs& limbs, std::uint32_t divisor) {
  std::uint64_t rest = 0;
  for (std::size_t i = limbs.size(); i-- > 0;) {
    const std::uint64_t current = (rest << kLimbBits) | limbs[i];
    limbs[i] = static_cast<std::uint32_t>(current / divisor);
    rest = current % divisor;
  }
  trim(limbs);
  return static_cast<std::uint32_t>(rest);
}

BigQuotient divide_truncate(const BigInt& a, const BigInt& b) {
  MagnitudeQuotient parts = divide_magnitude(BigIntAccess::limbs(a), BigIntAccess::limbs(b));
  return {BigIntAccess::make(std::move(parts.quotient), a.negative() != b.negative()),
          BigIntAccess::make(std::move(parts.remainder), a.negative())};
}

// 切り捨ての結果から作る。余りの符号が除数と違うときだけ、商を 1 つ下げて余りに除数を足す。
BigQuotient divide_floor(const BigInt& a, const BigInt& b) {
  BigQuotient result = divide_truncate(a, b);
  if (!result.remainder.is_zero() && result.remainder.negative() != b.negative()) {
    result.quotient = result.quotient - BigInt(1);
    result.remainder = result.remainder + b;
  }
  return result;
}

// Euclid の互除法。余りは前の除数より小さいので、止まる。
BigInt gcd(const BigInt& a, const BigInt& b) {
  BigInt x = a.negative() ? -a : a;
  BigInt y = b.negative() ? -b : b;
  while (!y.is_zero()) {
    BigInt rest = divide_truncate(x, y).remainder;
    x = std::move(y);
    y = std::move(rest);
  }
  return x;
}

// 指数の上のビットから 2 乗と掛け算を繰り返す。掛ける相手がいつも base なので、大きな数どうしの積は 2 乗だけになる。
BigInt power(const BigInt& base, std::uint64_t exponent) {
  BigInt result(1);
  for (auto bit = static_cast<int>(std::bit_width(exponent)) - 1; bit >= 0; --bit) {
    result = result * result;
    if (((exponent >> bit) & 1U) != 0) result = result * base;
  }
  return result;
}

}  // namespace mosa
