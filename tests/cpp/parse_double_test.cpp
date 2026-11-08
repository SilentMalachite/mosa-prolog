// mosa::parse_double（src/platform/parse_double.cpp）が std::from_chars(first, last, double&) と同じ三つの場合と
// 同じビット列を返すかを見る。期待値はこの機械の既定の配備対象で from_chars を一度だけ呼んで作った表を貼った。
// Apple の配備対象 14.0 でも同じ表を通すので、ここでは浮動小数点の from_chars を呼ばない（呼ぶとビルドできない）。
#include "harness.hpp"
#include "mosa/parse_double.hpp"

#include <array>
#include <bit>
#include <clocale>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

using K = mosa::DoubleKind;

struct Case {
  std::string_view text;
  K kind;
  std::uint64_t bits;  // K::Value のときだけ比べる
};

constexpr std::array kCases{
    // ふつうの値
    Case{"1.0", K::Value, 0x3ff0000000000000},
    Case{"0.0", K::Value, 0x0000000000000000},
    Case{"-0.0", K::Value, 0x8000000000000000},
    Case{"-2.5", K::Value, 0xc004000000000000},
    Case{"0.1", K::Value, 0x3fb999999999999a},
    Case{"0.5", K::Value, 0x3fe0000000000000},
    Case{"3.14159", K::Value, 0x400921f9f01b866e},
    Case{"123.456", K::Value, 0x405edd2f1a9fbe77},
    Case{"1.0e10", K::Value, 0x4202a05f20000000},
    Case{"1.5e-5", K::Value, 0x3eef75104d551d69},
    Case{"1.0E+2", K::Value, 0x4059000000000000},
    Case{"6.02214076e23", K::Value, 0x44dfe185ca57c517},
    Case{"1.", K::Value, 0x3ff0000000000000},
    Case{".5", K::Value, 0x3fe0000000000000},
    // 最大の有限値と、そのすぐ上（上位の溢れ）
    Case{"1.7976931348623157e308", K::Value, 0x7fefffffffffffff},
    Case{"1.7976931348623158e308", K::Value, 0x7fefffffffffffff},
    Case{"1.7976931348623159e308", K::OutOfRange, 0},
    Case{"1.0e309", K::OutOfRange, 0},
    Case{"1e99999999999999999999", K::OutOfRange, 0},
    // 最小の正規化数と、その下の非正規化数（Apple の strtod_l は非正規化数で ERANGE を立てるが値は返す）
    Case{"2.2250738585072014e-308", K::Value, 0x0010000000000000},
    Case{"2.2250738585072012e-308", K::Value, 0x0010000000000000},
    Case{"2.2250738585072011e-308", K::Value, 0x000fffffffffffff},
    Case{"1.0e-310", K::Value, 0x000012688b70e62b},
    Case{"1.0e-320", K::Value, 0x00000000000007e8},
    // 最小の非正規化数と、そのすぐ下（下位の溢れ）。0 は溢れではない
    Case{"4.9e-324", K::Value, 0x0000000000000001},
    Case{"5.0e-324", K::Value, 0x0000000000000001},
    Case{"3.0e-324", K::Value, 0x0000000000000001},
    Case{"2.4703282292062328e-324", K::Value, 0x0000000000000001},
    Case{"2.4703282292062327e-324", K::OutOfRange, 0},
    Case{"2.4e-324", K::OutOfRange, 0},
    Case{"1.0e-400", K::OutOfRange, 0},
    Case{"0.0e999", K::Value, 0x0000000000000000},
    Case{"0.0e-999", K::Value, 0x0000000000000000},
    // 正しい丸めに長い桁が要るもの（ちょうど中間は偶数へ）
    Case{"9007199254740993.0", K::Value, 0x4340000000000000},
    Case{"9007199254740995.0", K::Value, 0x4340000000000002},
    Case{"0.1000000000000000055511151231257827021181583404541015625", K::Value, 0x3fb999999999999a},
    Case{"1.00000000000000011102230246251565404236316680908203125", K::Value, 0x3ff0000000000000},
    Case{"1.00000000000000011102230246251565404236316680908203126", K::Value, 0x3ff0000000000001},
    Case{"123456789012345678901234567890.0", K::Value, 0x45f8ee90ff6c373e},
    // strtod は読むが from_chars は拒むもの、と読み残し
    Case{"", K::Invalid, 0},
    Case{" 1.0", K::Invalid, 0},
    Case{"\t1.0", K::Invalid, 0},
    Case{"+1.0", K::Invalid, 0},
    Case{"0x1p3", K::Invalid, 0},
    Case{"0x10", K::Invalid, 0},
    Case{"1.0 ", K::Invalid, 0},
    Case{"1.0x", K::Invalid, 0},
    Case{"1e", K::Invalid, 0},
    Case{"1e+", K::Invalid, 0},
    Case{".", K::Invalid, 0},
    Case{"-", K::Invalid, 0},
    Case{"e5", K::Invalid, 0},
    Case{"1..0", K::Invalid, 0},
    Case{"1,5", K::Invalid, 0},
    Case{"1_0.0", K::Invalid, 0},
    // 読んだ頭が範囲外なら、読み残しより範囲外が先（make_float が先に範囲外を見る順と同じ）
    Case{"1e999x", K::OutOfRange, 0},
    Case{"1e-999x", K::OutOfRange, 0},
    // from_chars も inf と nan は読む。nan の括弧の中は捨てる（strtod_l はそれを payload に入れる）
    Case{"inf", K::Value, 0x7ff0000000000000},
    Case{"INF", K::Value, 0x7ff0000000000000},
    Case{"infinity", K::Value, 0x7ff0000000000000},
    Case{"-inf", K::Value, 0xfff0000000000000},
    Case{"infin", K::Invalid, 0},
    Case{"nan", K::Value, 0x7ff8000000000000},
    Case{"NaN", K::Value, 0x7ff8000000000000},
    Case{"-nan", K::Value, 0xfff8000000000000},
    Case{"nan(123)", K::Value, 0x7ff8000000000000},
    Case{"nan()", K::Value, 0x7ff8000000000000},
    Case{"nan(", K::Invalid, 0},
};

std::string label_of(const Case& c, std::string_view what) {
  return std::string(what) + " for \"" + std::string(c.text) + "\"";
}

void table_matches_from_chars() {
  for (const Case& c : kCases) {
    // 終わりの NUL に頼らないことも見るため、後ろに字を足した文字列の部分を渡す。
    const std::string padded = std::string(c.text) + "9";
    const mosa::DoubleParse parsed = mosa::parse_double(std::string_view(padded).substr(0, c.text.size()));
    mosa_test::check(parsed.kind == c.kind, label_of(c, "kind"));
    if (c.kind == K::Value) {
      mosa_test::check(std::bit_cast<std::uint64_t>(parsed.value) == c.bits, label_of(c, "bits"));
    }
  }
}

// 利用者のロケールが小数点を変えないこと。de_DE が無い機械では setlocale が失敗し、C のまま同じ確かめになる。
void locale_does_not_change_point() {
  const bool german = std::setlocale(LC_ALL, "de_DE.UTF-8") != nullptr;
  const mosa::DoubleParse point = mosa::parse_double("1.5");
  const mosa::DoubleParse comma = mosa::parse_double("1,5");
  std::setlocale(LC_ALL, "C");
  const std::string where = german ? " under de_DE.UTF-8" : " under C";
  mosa_test::check(point.kind == K::Value && std::bit_cast<std::uint64_t>(point.value) == 0x3ff8000000000000,
                   "1.5 reads with a point" + where);
  mosa_test::check(comma.kind == K::Invalid, "1,5 is not a number" + where);
}

}  // namespace

int main() {
  table_matches_from_chars();
  locale_does_not_change_point();
  return mosa_test::failures() == 0 ? 0 : 1;
}
