// 浮動小数点数の綴りを double にする（リリース 0.1.0 Task 10b）。OS ごとの分岐はここだけに書く。
// Apple の libc++ は浮動小数点の std::from_chars を macOS 26.0 からとしていて、配備対象 14.0 ではビルドできない。
// Apple だけ strtod_l と C ロケールで from_chars と同じ結果を作り、ほかの OS は from_chars のまま変えない。
// テスト: tests/cpp/parse_double_test.cpp
#include "mosa/parse_double.hpp"

#include <cstddef>

#if defined(__APPLE__)
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
// xlocale.h は stdlib.h が先に読まれているときだけ strtod_l を宣言するので、<cstdlib> より後に置く。
#include <xlocale.h>
#else
#include <charconv>
#include <system_error>
#endif

namespace mosa {

namespace {

#if defined(__APPLE__)

bool ascii_digit(char ch) { return ch >= '0' && ch <= '9'; }

// C の n-char-sequence の字（数字、英字、_）。
bool n_char(char ch) {
  return ascii_digit(ch) || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

std::size_t skip_digits(std::string_view text, std::size_t at) {
  while (at < text.size() && ascii_digit(text[at])) ++at;
  return at;
}

// 英字の大文字小文字を問わず word で始まるか。word は小文字で渡す。
bool starts_with_word(std::string_view text, std::string_view word) {
  if (text.size() < word.size()) return false;
  for (std::size_t i = 0; i < word.size(); ++i) {
    const char ch = text[i];
    const char lower = ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
    if (lower != word[i]) return false;
  }
  return true;
}

// 仮数部に数字が一つも無ければ 0。指数部は後ろに数字があるときだけ読む（from_chars は "1e" を "1" まで読む）。
std::size_t match_decimal(std::string_view text) {
  const std::size_t whole = skip_digits(text, 0);
  std::size_t at = whole;
  bool any = whole > 0;
  if (at < text.size() && text[at] == '.') {
    const std::size_t fraction = skip_digits(text, at + 1);
    any = any || fraction > at + 1;
    at = fraction;
  }
  if (!any) return 0;
  if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
    std::size_t sign = at + 1;
    if (sign < text.size() && (text[sign] == '+' || text[sign] == '-')) ++sign;
    const std::size_t exponent = skip_digits(text, sign);
    if (exponent > sign) at = exponent;
  }
  return at;
}

// nan の後ろの括弧は閉じるところまで n-char だけのときに限って読む。
std::size_t match_nan(std::string_view text) {
  constexpr std::size_t word = 3;
  if (text.size() <= word || text[word] != '(') return word;
  std::size_t at = word + 1;
  while (at < text.size() && n_char(text[at])) ++at;
  return at < text.size() && text[at] == ')' ? at + 1 : word;
}

struct Match {
  std::size_t length = 0;  // from_chars が読む頭の長さ。0 なら読めない
  bool nan = false;
};

// from_chars の general の形は C ロケールの strtod の形から、先頭の空白、+ の符号、0x の 16 進を除いたもの。
// strtod_l はそれらも読むので、先にこの形で頭を切り、strtod_l にはその頭だけを渡す。
Match match_from_chars(std::string_view text) {
  const std::size_t sign = text.starts_with('-') ? 1 : 0;
  const std::string_view body = text.substr(sign);
  if (starts_with_word(body, "nan")) return {sign + match_nan(body), true};
  if (starts_with_word(body, "infinity")) return {sign + 8, false};
  if (starts_with_word(body, "inf")) return {sign + 3, false};
  const std::size_t decimal = match_decimal(body);
  return {decimal == 0 ? 0 : sign + decimal, false};
}

// 一度だけ作る。作れずに空の locale_t になっても、Apple の _l の関数は空を C ロケールとして扱う（xlocale(3)）。
locale_t c_locale() {
  static const locale_t locale = newlocale(LC_ALL_MASK, "C", nullptr);
  return locale;
}

// 頭は from_chars の形に合うので、strtod_l もちょうど頭の終わりまで読む。errno は from_chars と同じく残さない。
double strtod_head(std::string_view head, int& range_error) {
  const std::string copy(head);  // string_view は NUL で終わらない
  const int saved = errno;
  errno = 0;
  const double value = strtod_l(copy.c_str(), nullptr, c_locale());
  range_error = errno;
  errno = saved;
  return value;
}

DoubleParse parse_with_strtod(std::string_view text) {
  const Match match = match_from_chars(text);
  if (match.length == 0) return {};
  const bool whole = match.length == text.size();
  if (match.nan) {
    // strtod_l は括弧の中の数を payload に入れるが、from_chars は捨てて符号だけを残す。
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (!whole) return {};
    return {DoubleKind::Value, text.starts_with('-') ? -nan : nan};
  }
  int range_error = 0;
  const double value = strtod_head(text.substr(0, match.length), range_error);
  // Apple の strtod_l は 0 でない非正規化数の結果にも ERANGE を立てる（macOS 27.2 で確かめた。gdtoa の strtod も
  // 指数部が 0 の結果に ERANGE を立てる）。from_chars はそれを値として返すので、
  // 範囲外は無限大か 0 になったときだけにする（4.9e-324 は ERANGE とビット列 0x1 を、1.0e-400 は ERANGE と 0 を返す）。
  if (range_error == ERANGE && (value == 0.0 || std::isinf(value))) return {DoubleKind::OutOfRange, 0.0};
  if (!whole) return {};
  return {DoubleKind::Value, value};
}

#else

DoubleParse parse_with_from_chars(std::string_view text) {
  double value = 0.0;
  const char* const last = text.data() + text.size();
  const std::from_chars_result parsed = std::from_chars(text.data(), last, value);
  if (parsed.ec == std::errc::result_out_of_range) return {DoubleKind::OutOfRange, 0.0};
  if (parsed.ec != std::errc{} || parsed.ptr != last) return {};
  return {DoubleKind::Value, value};
}

#endif

}  // namespace

DoubleParse parse_double(std::string_view text) {
#if defined(__APPLE__)
  return parse_with_strtod(text);
#else
  return parse_with_from_chars(text);
#endif
}

}  // namespace mosa
