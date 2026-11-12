// BigInt（include/mosa/bigint.hpp）の単体テスト。期待値は Python の整数で確かめた。
// 計画のタスク 2 の 11 の検査。各検査の中の追加の行は、境界（0 の符号、INT64_MIN、語の境目の繰り上がり、
// Algorithm D の足し戻し、上位の 0 の語の正規化）を押さえる。

#include "harness.hpp"
#include "mosa/bigint.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa::BigInt;

constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::uint64_t kAllOnes = ~std::uint64_t{0};

// shift_left を試す側でも使うので、2^bits は from_words で作り、試す関数に頼らない。
BigInt pow2(unsigned bits) {
  std::vector<std::uint64_t> words(bits / 64 + 1, 0);
  words.back() = std::uint64_t{1} << (bits % 64);
  return BigInt::from_words(words, false);
}

BigInt decimal(std::string_view text) {
  const bool negative = text.starts_with('-');
  const auto magnitude = mosa::parse_magnitude(negative ? text.substr(1) : text, 10);
  mosa_test::check(magnitude.has_value(), "decimal helper parses");
  if (!magnitude) return BigInt();
  return negative ? -*magnitude : *magnitude;
}

// 試す実装とは別の 2 乗の掛け算。64 ビットの語を 32 ビットに割り、積を 64 ビットに収める。
BigInt schoolbook_multiply(const BigInt& a, const BigInt& b) {
  const auto halves = [](const std::vector<std::uint64_t>& words) {
    std::vector<std::uint32_t> out;
    for (const std::uint64_t word : words) {
      out.push_back(static_cast<std::uint32_t>(word));
      out.push_back(static_cast<std::uint32_t>(word >> 32));
    }
    return out;
  };
  const auto x = halves(a.words());
  const auto y = halves(b.words());
  std::vector<std::uint32_t> product(x.size() + y.size() + 1, 0);
  for (std::size_t i = 0; i < x.size(); ++i) {
    std::uint64_t carry = 0;
    for (std::size_t j = 0; j < y.size(); ++j) {
      const std::uint64_t t = std::uint64_t{x[i]} * y[j] + product[i + j] + carry;
      product[i + j] = static_cast<std::uint32_t>(t);
      carry = t >> 32;
    }
    product[i + y.size()] = static_cast<std::uint32_t>(carry);
  }
  std::vector<std::uint64_t> words;
  for (std::size_t i = 0; i + 1 < product.size(); i += 2) {
    words.push_back(std::uint64_t{product[i]} | (std::uint64_t{product[i + 1]} << 32));
  }
  return BigInt::from_words(words, a.negative() != b.negative());
}

// 語の境目の繰り上がりを踏むよう、乱数の語に 0、全ビット 1、最上位だけの 1 を混ぜる。
std::vector<std::uint64_t> random_words(std::mt19937_64& gen, std::size_t count, bool biased) {
  std::vector<std::uint64_t> words(count);
  for (auto& word : words) {
    word = gen();
    if (!biased) continue;
    switch (word % 5) {
      case 0: word = 0; break;
      case 1: word = kAllOnes; break;
      case 2: word = std::uint64_t{1} << 63; break;
      case 3: word = 0x8000'0000ULL; break;
      default: break;
    }
  }
  if (!words.empty() && words.back() == 0) words.back() = 1;
  return words;
}

void check_round_trip() {
  const std::string text = "1267650600228229401496703205376";
  const auto value = mosa::parse_magnitude(text, 10);
  mosa_test::check(value && mosa::to_text(*value) == text, "round_trip decimal");
  mosa_test::check(value && *value == pow2(100), "round_trip decimal is 2^100");
  const auto hex = mosa::parse_magnitude("ffffffffffffffffffff", 16);
  mosa_test::check(hex && mosa::to_text(*hex, 16) == "ffffffffffffffffffff", "round_trip hex");
  mosa_test::check(hex && hex->words() == std::vector<std::uint64_t>{kAllOnes, 0xFFFF}, "round_trip hex words");
  const auto upper = mosa::parse_magnitude("FFFFFFFFFFFFFFFFFFFF", 16);
  mosa_test::check(upper && hex && *upper == *hex, "round_trip hex upper case");
  const auto octal = mosa::parse_magnitude("777", 8);
  mosa_test::check(octal && *octal == BigInt(511) && mosa::to_text(BigInt(-511), 8) == "-777", "round_trip octal");
  const auto binary = mosa::parse_magnitude(std::string(130, '1'), 2);
  mosa_test::check(binary && *binary == pow2(130) - BigInt(1) && mosa::to_text(*binary, 2) == std::string(130, '1'),
                   "round_trip binary");
  mosa_test::check(!mosa::parse_magnitude("12a", 10) && !mosa::parse_magnitude("8", 8) &&
                       !mosa::parse_magnitude("2", 2) && !mosa::parse_magnitude("g", 16) &&
                       !mosa::parse_magnitude("", 10),
                   "round_trip rejects non digits");
  const auto zeros = mosa::parse_magnitude("0000000000000000000000000000000000000000000000000007", 10);
  mosa_test::check(zeros && *zeros == BigInt(7) && zeros->words() == std::vector<std::uint64_t>{7},
                   "round_trip leading zeros normalize");
  mosa_test::check(mosa::to_text(BigInt()) == "0" && mosa::to_text(-pow2(100)) == "-" + text,
                   "round_trip zero and negative");
}

void check_int64_edges() {
  mosa_test::check(BigInt(kMin).to_int64() == kMin, "int64_edges min round trips");
  mosa_test::check(!(BigInt(kMax) + BigInt(1)).to_int64().has_value(), "int64_edges max plus one");
  mosa_test::check(mosa::to_text(BigInt(kMax) + BigInt(1)) == "9223372036854775808", "int64_edges max plus one text");
  mosa_test::check(mosa::to_text(BigInt(kMin) - BigInt(1)) == "-9223372036854775809", "int64_edges min minus one");
  mosa_test::check(mosa::to_text(BigInt(kMax) * BigInt(2)) == "18446744073709551614", "int64_edges max times two");
  mosa_test::check(!(BigInt(kMin) - BigInt(1)).to_int64() && (BigInt(kMax) + BigInt(1) - BigInt(1)).to_int64() == kMax,
                   "int64_edges back inside");
  mosa_test::check(BigInt(kMin).words() == std::vector<std::uint64_t>{std::uint64_t{1} << 63} &&
                       BigInt(kMin).negative() && BigInt(kMin).bit_length() == 64 && mosa::to_text(BigInt(kMin)) ==
                       "-9223372036854775808",
                   "int64_edges min magnitude");
  mosa_test::check(BigInt().is_zero() && !BigInt().negative() && BigInt().words().empty() &&
                       BigInt().bit_length() == 0 && BigInt(0) == BigInt(),
                   "int64_edges zero");
  mosa_test::check(!(BigInt(5) - BigInt(5)).negative() && !(-BigInt()).negative() &&
                       !(BigInt(-5) + BigInt(5)).negative() && !(BigInt(-5) * BigInt(0)).negative(),
                   "int64_edges zero is not negative");
  mosa_test::check(BigInt(3) - BigInt(5) == BigInt(-2) && BigInt(-3) + BigInt(5) == BigInt(2) &&
                       BigInt(-3) - BigInt(-5) == BigInt(2) && BigInt(-3) * BigInt(-5) == BigInt(15),
                   "int64_edges signs across zero");
  const BigInt two_words = BigInt::from_words(std::vector<std::uint64_t>{kAllOnes, kAllOnes}, false);
  const BigInt three_words = BigInt::from_words(std::vector<std::uint64_t>{0, 0, 1}, false);
  mosa_test::check(two_words + BigInt(1) == three_words && three_words - BigInt(1) == two_words,
                   "int64_edges carry and borrow across words");
  mosa_test::check(three_words - two_words == BigInt(1) && (three_words - two_words).words().size() == 1,
                   "int64_edges subtraction drops top words");
  const BigInt padded = BigInt::from_words(std::vector<std::uint64_t>{5, 0, 0}, true);
  const BigInt negative_zero = BigInt::from_words(std::vector<std::uint64_t>{0, 0}, true);
  mosa_test::check(padded == BigInt(-5) && padded.words().size() == 1, "int64_edges from_words drops zero words");
  mosa_test::check(negative_zero.is_zero() && !negative_zero.negative() && negative_zero == BigInt(),
                   "int64_edges from_words zero is positive");
  const BigInt p = pow2(100);
  mosa_test::check(BigInt(-1) < BigInt(0) && BigInt(0) < BigInt(1) && -p < BigInt(kMin) && p > BigInt(kMax) &&
                       -p < -BigInt(kMax) && p + BigInt(1) > p && -(p + BigInt(1)) < -p && p.bit_length() == 101,
                   "int64_edges ordering");
}

void expect_division_identity(const BigInt& a, const BigInt& b, std::string_view label) {
  const auto t = mosa::divide_truncate(a, b);
  const auto f = mosa::divide_floor(a, b);
  const auto magnitude = [](const BigInt& x) { return x.negative() ? -x : x; };
  const bool t_sign = t.remainder.is_zero() || t.remainder.negative() == a.negative();
  const bool f_sign = f.remainder.is_zero() || f.remainder.negative() == b.negative();
  mosa_test::check(t.quotient * b + t.remainder == a && magnitude(t.remainder) < magnitude(b) && t_sign, label);
  mosa_test::check(f.quotient * b + f.remainder == a && magnitude(f.remainder) < magnitude(b) && f_sign, label);
}

void check_div_signs() {
  const BigInt p = pow2(100);
  const auto t = mosa::divide_truncate(-p, BigInt(7));
  mosa_test::check(t.quotient == decimal("-181092942889747057356671886482") && t.remainder == BigInt(-2),
                   "div_signs truncate");
  const auto f = mosa::divide_floor(-p, BigInt(7));
  mosa_test::check(f.quotient == decimal("-181092942889747057356671886483") && f.remainder == BigInt(5),
                   "div_signs floor");
  // Algorithm D で、商の見積もりが 1 大きく、足し戻しが要る組。
  const auto add_back = mosa::divide_truncate(pow2(95) + BigInt(3), pow2(93) + BigInt(1));
  mosa_test::check(add_back.quotient == BigInt(3) && add_back.remainder == pow2(93), "div_signs add back");
  const auto exact = mosa::divide_floor(p * p, -p);
  mosa_test::check(exact.quotient == -p && exact.remainder.is_zero() && !exact.remainder.negative(),
                   "div_signs exact");
  const auto small = mosa::divide_floor(BigInt(5), p);
  mosa_test::check(small.quotient.is_zero() && small.remainder == BigInt(5), "div_signs dividend smaller");
  const auto below = mosa::divide_floor(BigInt(-5), p);
  mosa_test::check(below.quotient == BigInt(-1) && below.remainder == p - BigInt(5), "div_signs floor below");
  std::mt19937_64 gen(20261112);
  for (int round = 0; round < 200; ++round) {
    const auto a = random_words(gen, 1 + gen() % 9, true);
    const auto b = random_words(gen, 1 + gen() % 5, true);
    const BigInt x = BigInt::from_words(a, (gen() & 1) != 0);
    const BigInt y = BigInt::from_words(b, (gen() & 1) != 0);
    expect_division_identity(x, y, "div_signs identity");
    expect_division_identity(x * y + BigInt(1), y, "div_signs identity near multiple");
    expect_division_identity(x * y - BigInt(1), y, "div_signs identity below multiple");
  }
}

void check_shifts() {
  const BigInt p = pow2(100);
  mosa_test::check(mosa::shift_right(-p, 1) == -pow2(99), "shifts negative right");
  mosa_test::check(mosa::shift_right(p, 101).is_zero(), "shifts all out");
  mosa_test::check(mosa::shift_right(-p, 101) == BigInt(-1), "shifts negative all out");
  mosa_test::check(mosa::shift_left(BigInt(1), 64) == pow2(64), "shifts left 64");
  mosa_test::check(mosa::shift_right(-(p + BigInt(1)), 100) == BigInt(-2) && mosa::shift_right(BigInt(-5), 1) ==
                       BigInt(-3) && mosa::shift_right(BigInt(-1), 1000) == BigInt(-1) &&
                       mosa::shift_right(BigInt(-1), 1) == BigInt(-1),
                   "shifts floor of negatives");
  mosa_test::check(mosa::shift_left(-p, 3) == -pow2(103) && mosa::shift_left(BigInt(), 1000).is_zero() &&
                       mosa::shift_left(p, 0) == p && mosa::shift_right(p, 0) == p,
                   "shifts identity and zero");
  bool across = true;
  for (unsigned bits = 0; bits < 200; ++bits) {
    const BigInt value = pow2(bits) + pow2(bits / 2);
    across = across && mosa::shift_left(BigInt(3), bits) == pow2(bits + 1) + pow2(bits) &&
             mosa::shift_right(mosa::shift_left(value, 37), 37) == value &&
             mosa::shift_right(value, bits) == BigInt(bits == 0 ? 2 : 1);
  }
  mosa_test::check(across, "shifts across word boundaries");
}

void check_bits() {
  const BigInt q = pow2(70);
  mosa_test::check(mosa::bit_not(q) == decimal("-1180591620717411303425"), "bits not");
  mosa_test::check(mosa::bit_and(-q, BigInt(255)).is_zero(), "bits and negative");
  mosa_test::check(mosa::bit_and(mosa::bit_not(q), BigInt(255)) == BigInt(255), "bits and not");
  mosa_test::check(mosa::bit_xor(q, q + BigInt(5)) == BigInt(5), "bits xor");
  mosa_test::check(mosa::bit_or(-q, BigInt(1)) == decimal("-1180591620717411303423"), "bits or negative");
  mosa_test::check(mosa::bit_and(-(q + BigInt(3)), pow2(72) - BigInt(1)) == decimal("3541774862152233910269"),
                   "bits and mixed widths");
  mosa_test::check(mosa::bit_xor(-pow2(64), pow2(64) - BigInt(1)) == BigInt(-1) &&
                       mosa::bit_or(q, BigInt(-1)) == BigInt(-1) && mosa::bit_and(BigInt(-1), q) == q &&
                       mosa::bit_and(-q, -q) == -q && mosa::bit_xor(BigInt(-1), q) == mosa::bit_not(q),
                   "bits with minus one");
  mosa_test::check(mosa::bit_not(BigInt(-1)).is_zero() && mosa::bit_not(BigInt()) == BigInt(-1) &&
                       mosa::bit_not(mosa::bit_not(-q)) == -q && mosa::bit_xor(-q, -q).is_zero() &&
                       !mosa::bit_xor(-q, -q).negative(),
                   "bits not and zero");
}

void check_gcd_pow() {
  const BigInt p = pow2(100);
  mosa_test::check(mosa::gcd(p, mosa::power(BigInt(6), 50)) == BigInt(1125899906842624), "gcd_pow gcd");
  mosa_test::check(mosa::power(BigInt(2), 100) == p, "gcd_pow power");
  mosa_test::check(p * p == decimal("1606938044258990275541962092341162602522202993782792835301376"), "gcd_pow square");
  mosa_test::check(mosa::gcd(BigInt(), BigInt()).is_zero() && mosa::gcd(BigInt(-12), BigInt(18)) == BigInt(6) &&
                       mosa::gcd(-p, BigInt()) == p && mosa::gcd(BigInt(), -p) == p &&
                       mosa::gcd(-p, -mosa::power(BigInt(6), 50)) == BigInt(1125899906842624),
                   "gcd_pow gcd signs");
  mosa_test::check(mosa::power(BigInt(), 0) == BigInt(1) && mosa::power(-p, 0) == BigInt(1) &&
                       mosa::power(BigInt(), 5).is_zero() && mosa::power(BigInt(-3), 3) == BigInt(-27) &&
                       mosa::power(BigInt(-2), 101) == -pow2(101) && mosa::power(BigInt(-1), kAllOnes) == BigInt(-1),
                   "gcd_pow power edges");
}

void check_to_double() {
  const auto near = [](const BigInt& value, double expected) {
    const auto result = mosa::to_double(value);
    return result.has_value() && *result == expected;
  };
  mosa_test::check(near(pow2(53) + BigInt(1), 9007199254740992.0), "to_double tie to even down");
  mosa_test::check(near(pow2(54) + BigInt(3), 18014398509481988.0), "to_double nearest");
  const BigInt below_max = pow2(1024) - pow2(970) - BigInt(1);
  mosa_test::check(near(below_max, 1.7976931348623157e308), "to_double largest finite");
  mosa_test::check(!mosa::to_double(pow2(1024) - pow2(970)).has_value(), "to_double rounds to overflow");
  mosa_test::check(!mosa::to_double(pow2(1024)).has_value() && !mosa::to_double(-pow2(1024)).has_value(),
                   "to_double out of range");
  mosa_test::check(near(BigInt(), 0.0) && near(-(pow2(53) + BigInt(1)), -9007199254740992.0) &&
                       near(BigInt(kMin), -9223372036854775808.0) && near(pow2(1023), std::ldexp(1.0, 1023)) &&
                       near(-below_max, -1.7976931348623157e308),
                   "to_double signs and powers");
  mosa_test::check(near(pow2(54) + BigInt(2), 18014398509481984.0) && near(pow2(54) + BigInt(6), 18014398509481992.0),
                   "to_double ties to even");
  // 上位 64 ビットの外にある 1 が、ちょうど半分の値を上へ寄せる（sticky）。
  mosa_test::check(near(pow2(100) + pow2(47) + BigInt(1), std::ldexp(1.0, 100) + std::ldexp(1.0, 48)) &&
                       near(pow2(100) + pow2(47), std::ldexp(1.0, 100)),
                   "to_double sticky bits");
}

void check_from_double() {
  mosa_test::check(mosa::to_text(mosa::from_integral_double(1.0e32)) == "100000000000000005366162204393472",
                   "from_double 1e32");
  mosa_test::check(mosa::from_integral_double(-1.0e20) == decimal("-100000000000000000000"), "from_double -1e20");
  const BigInt zero = mosa::from_integral_double(-0.0);
  mosa_test::check(mosa::from_integral_double(0.0).is_zero() && zero.is_zero() && !zero.negative(),
                   "from_double zero");
  mosa_test::check(mosa::from_integral_double(1.0) == BigInt(1) && mosa::from_integral_double(-3.0) == BigInt(-3) &&
                       mosa::from_integral_double(-9223372036854775808.0) == BigInt(kMin) &&
                       mosa::from_integral_double(std::ldexp(1.0, 1023)) == pow2(1023),
                   "from_double small and powers");
  const double largest = std::numeric_limits<double>::max();
  const BigInt exact = mosa::from_integral_double(largest);
  mosa_test::check(exact == pow2(1024) - pow2(971) && mosa::to_double(exact) == largest, "from_double largest");
}

void check_karatsuba() {
  static_assert(mosa::kKaratsubaWords < 500, "1,000 語と 1,500 語の積が、釣り合わない分け方まで Karatsuba に入る");
  std::mt19937_64 gen(13211);
  const BigInt a = BigInt::from_words(random_words(gen, 1000, false), false);
  const BigInt b = BigInt::from_words(random_words(gen, 1500, false), true);
  const BigInt expected = schoolbook_multiply(a, b);
  mosa_test::check(a * b == expected && b * a == expected, "karatsuba random");
  mosa_test::check(a * a == schoolbook_multiply(a, a), "karatsuba square");
  const BigInt ones = BigInt::from_words(std::vector<std::uint64_t>(600, kAllOnes), false);
  const BigInt other = BigInt::from_words(std::vector<std::uint64_t>(700, kAllOnes), false);
  mosa_test::check(ones * other == schoolbook_multiply(ones, other), "karatsuba all ones");
  const BigInt mixed = BigInt::from_words(random_words(gen, 1200, true), false);
  const BigInt short_one = BigInt::from_words(random_words(gen, 90, true), true);
  mosa_test::check(mixed * short_one == schoolbook_multiply(mixed, short_one) &&
                       mixed * other == schoolbook_multiply(mixed, other),
                   "karatsuba unbalanced");
}

void check_factorial() {
  BigInt value(1);
  for (std::int64_t i = 2; i <= 1000; ++i) value = value * BigInt(i);
  const std::string text = mosa::to_text(value);
  mosa_test::check(text.size() == 2568, "factorial digits");
  mosa_test::check(text.starts_with("40238726007709377354"), "factorial head");
  mosa_test::check(text.ends_with("0000000000"), "factorial tail");
  mosa_test::check(mosa::parse_magnitude(text, 10) == value, "factorial parses back");
  BigInt back = value;
  for (std::int64_t i = 1000; i >= 2; --i) back = mosa::divide_truncate(back, BigInt(i)).quotient;
  mosa_test::check(back == BigInt(1), "factorial divides back");
}

void check_key() {
  const BigInt p = pow2(100);
  const auto words = p.words();
  const auto copy = (p + BigInt(1) - BigInt(1)).words();
  mosa_test::check(mosa::big_key(false, words) == mosa::big_key(false, copy), "key equal values");
  const std::vector<std::uint64_t> low_one{1, 5};
  const std::vector<std::uint64_t> low_two{2, 5};
  mosa_test::check(mosa::big_key(false, low_one) != mosa::big_key(false, low_two), "key lowest word differs");
  mosa_test::check(mosa::big_key(true, words) != mosa::big_key(false, words), "key sign differs");
  // 符号のバイト、続いて各語を下位のバイトから入れた FNV-1a 64（Python で計算）。mosac と式がずれれば止まる。
  mosa_test::check(mosa::big_key(false, words) == 0x4da50d85355360afULL &&
                       mosa::big_key(true, words) == 0xfecdc98cc87b2a3cULL,
                   "key pinned values");
}

}  // namespace

int main() {
  check_round_trip();
  check_int64_edges();
  check_div_signs();
  check_shifts();
  check_bits();
  check_gcd_pow();
  check_to_double();
  check_from_double();
  check_karatsuba();
  check_factorial();
  check_key();
  return mosa_test::failures();
}
