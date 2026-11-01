#include "mosa/lexer.hpp"

#include "mosa/char_class.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace mosa {
namespace {

int digit_value(char32_t code, int base) {
  int value = -1;
  if (code >= U'0' && code <= U'9') value = static_cast<int>(code - U'0');
  else if (code >= U'a' && code <= U'f') value = static_cast<int>(code - U'a') + 10;
  else if (code >= U'A' && code <= U'F') value = static_cast<int>(code - U'A') + 10;
  if (value < 0 || value >= base) return -1;
  return value;
}

// 乗算の前に 2^63 を見る。超えた整数は浮動小数にしない。
constexpr std::uint64_t kMinMagnitude = std::uint64_t{1} << 63U;

bool push_digit(std::uint64_t& value, int base, int digit) {
  const auto add = static_cast<std::uint64_t>(digit);
  if (value > (kMinMagnitude - add) / static_cast<std::uint64_t>(base)) return false;
  value = value * static_cast<std::uint64_t>(base) + add;
  return true;
}

bool code_point_ok(char32_t value) {
  return value <= 0x10FFFFU && (value < 0xD800U || value > 0xDFFFU);
}

std::optional<char32_t> symbolic_escape(char32_t code) {
  switch (code) {
    case U'a':
      return U'\u0007';
    case U'b':
      return U'\u0008';
    // 0'\e 0'\d 0'\s は Logtalk の名前。\n と \t の符号位置は変えない。
    case U'd':
      return U'\u007F';
    case U'e':
      return U'\u001B';
    case U'f':
      return U'\u000C';
    case U'n':
      return U'\n';
    case U'r':
      return U'\r';
    case U's':
      return U'\u0020';
    case U't':
      return U'\t';
    case U'v':
      return U'\u000B';
    case U'\\':
      return U'\\';
    case U'\'':
      return U'\'';
    case U'"':
      return U'"';
    case U'`':
      return U'`';
    default:
      return std::nullopt;
  }
}

// 拒むのは生の改行とタブだけ。垂直タブや改ページはここではない。
// CR LF は advance が 1 改行として両方消費する。LF だけ拒むと CR が改行を飲み込む。
bool raw_quote_break(char32_t code) {
  return code == U'\n' || code == U'\r' || code == U'\t';
}

// from_chars が範囲外と言った値の向きを、最初の 0 でない数字の桁と指数の和の符号で決める。指数の符号だけでは、
// 仮数部が 300 桁を超えると向きを誤る（M2 の敵対的レビュー A1/A2）。範囲外の値の桁は ±300 を超えるので、指数は
// 十億で頭打ちにしても符号は変わらない。clean は区切りの _ を除いた浮動小数点数の綴りである。
bool above_double(std::string_view clean) {
  const std::size_t mark = clean.find_first_of("eE");
  const std::string_view mantissa = clean.substr(0, mark);
  const std::size_t point = std::min(mantissa.find('.'), mantissa.size());
  const std::size_t first = mantissa.find_first_not_of("0.");
  if (first == std::string_view::npos) return false;
  const auto gap = [](std::size_t from, std::size_t to) { return static_cast<std::int64_t>(to - from); };
  std::int64_t place = first < point ? gap(first, point) - 1 : -gap(point, first);
  if (mark == std::string_view::npos) return place > 0;
  std::string_view exponent = clean.substr(mark + 1);
  const bool negative = exponent.starts_with('-');
  if (exponent.starts_with('-') || exponent.starts_with('+')) exponent.remove_prefix(1);
  constexpr std::int64_t cap = 1'000'000'000;
  std::int64_t value = 0;
  for (const char digit : exponent) value = std::min(cap, value * 10 + (digit - '0'));
  place += negative ? -value : value;
  return place > 0;
}

}  // namespace

Scan Lexer::scan_number() {
  const Place start = here();
  const SourceChar first = peek_char();
  if (first.code != U'0') return scan_decimal(start);
  const SourceChar second = peek_char_at(index_ + 1);
  if (!second.valid) return scan_decimal(start);
  if (second.code == U'\'') return scan_char_code(start);
  // 0b 0o 0x は小文字だけ。0X10 は十進の 0 と変数 X10 に分かれる。
  int base = 0;
  if (second.code == U'b') base = 2;
  else if (second.code == U'o') base = 8;
  else if (second.code == U'x') base = 16;
  if (base == 0 || !radix_ready(base)) return scan_decimal(start);
  return scan_radix(base, static_cast<char>(second.code), start);
}

bool Lexer::radix_ready(int base) const {
  const SourceChar digit = peek_char_at(index_ + 2);
  return digit.valid && digit_value(digit.code, base) >= 0;
}

std::optional<bool> Lexer::fraction_follows() const {
  const SourceChar dot = peek_char();
  if (!dot.valid || dot.code != U'.') return false;
  const SourceChar after = peek_char_at(index_ + dot.bytes);
  // `_` は桁と桁の間だけ置く。小数点の隣は別トークンに分けず、数の失敗にする。
  if (after.valid && after.code == U'_') return std::nullopt;
  return after.valid && digit_value(after.code, 10) >= 0;
}

Scan Lexer::scan_decimal(Place start) {
  const Digits whole = read_digits(10);
  if (whole.bad_separator) return fail_at(SyntaxReason::UnexpectedToken, whole.spelling, start);
  const std::optional<bool> fraction = fraction_follows();
  if (!fraction.has_value()) {
    return fail_at(SyntaxReason::UnexpectedToken, whole.spelling + "._", start);
  }
  if (*fraction) return finish_float(whole.spelling, start);
  if (whole.overflow) return fail_at(SyntaxReason::IntegerOverflow, whole.spelling, start);
  return integer_token(whole.value, whole.spelling, start);
}

// 2^63 の綴りは、- の後ろでないと読む側が溢れの誤りに使う。ほかの整数の綴りは今までどおり持たない。
Scan Lexer::integer_token(std::uint64_t magnitude, std::string spelling, Place start) {
  Scan scan = token_at(TokenKind::Integer, start);
  if (magnitude == kMinMagnitude) {
    scan.token.text = std::move(spelling);
    scan.token.needs_minus = true;
    scan.token.integer = std::numeric_limits<std::int64_t>::min();
  } else {
    scan.token.integer = static_cast<std::int64_t>(magnitude);
  }
  return scan;
}

Scan Lexer::scan_radix(int base, char prefix, Place start) {
  advance(U'0', 1);
  advance(static_cast<char32_t>(static_cast<unsigned char>(prefix)), 1);
  const Digits digits = read_digits(base);
  std::string spelling;
  spelling.push_back('0');
  spelling.push_back(prefix);
  spelling += digits.spelling;
  if (digits.bad_separator || !digits.any) {
    return fail_at(SyntaxReason::UnexpectedToken, std::move(spelling), start);
  }
  if (digits.overflow) return fail_at(SyntaxReason::IntegerOverflow, std::move(spelling), start);
  return integer_token(digits.value, std::move(spelling), start);
}

Scan Lexer::scan_char_code(Place start) {
  advance(U'0', 1);
  advance(U'\'', 1);
  // 0'（ は（の符号位置。引用の一種なので全角記号モードでも半角にしない。
  const FlagScope literal(fullwidth_, false);
  if (index_ >= source_.size()) return fail_at(SyntaxReason::UnexpectedToken, "0'", start);
  const SourceChar ch = peek_char();
  if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
  char32_t value = 0;
  if (ch.code == U'\\') {
    if (const std::optional<Scan> error = append_escape(nullptr, &value)) return *error;
  } else if (ch.code == U'\n' || ch.code == U'\r') {
    return fail_at(SyntaxReason::UnexpectedToken, "0'", start);
  } else if (ch.code == U'\'') {
    // 次も引用符なら ISO の二つ。そうでなければ一つ。どちらも 39 で、終止符は引用に入らない。
    advance(ch.code, ch.bytes);
    const SourceChar second = peek_char();
    if (second.valid && second.code == U'\'') advance(second.code, second.bytes);
    value = U'\'';
  } else {
    value = ch.code;
    advance(ch.code, ch.bytes);
  }
  Scan scan = token_at(TokenKind::Integer, start);
  scan.token.integer = static_cast<std::int64_t>(value);
  return scan;
}

Lexer::Digits Lexer::read_digits(int base) {
  Digits out;
  while (index_ < source_.size()) {
    const SourceChar ch = peek_char();
    if (!ch.valid) break;
    const int digit = digit_value(ch.code, base);
    if (digit >= 0) {
      out.any = true;
      out.spelling.push_back(static_cast<char>(ch.code));
      if (!out.overflow) out.overflow = !push_digit(out.value, base, digit);
      advance(ch.code, ch.bytes);
      continue;
    }
    // _ は同じ基数の桁と桁の間だけ。末尾や連続は、そこまで読んだ綴りで失敗する。
    if (ch.code != U'_') break;
    const SourceChar after = peek_char_at(index_ + ch.bytes);
    const int next = after.valid ? digit_value(after.code, base) : -1;
    out.spelling.push_back('_');
    advance(ch.code, ch.bytes);
    if (next < 0) {
      out.bad_separator = true;
      break;
    }
  }
  return out;
}

Scan Lexer::finish_float(std::string spelling, Place start) {
  const SourceChar dot = peek_char();
  advance(dot.code, dot.bytes);
  spelling.push_back('.');
  const Digits fraction = read_digits(10);
  spelling += fraction.spelling;
  if (fraction.bad_separator || !fraction.any) {
    return fail_at(SyntaxReason::UnexpectedToken, std::move(spelling), start);
  }
  if (const std::optional<Scan> error = take_exponent(spelling, start)) return *error;
  return make_float(spelling, start);
}

std::optional<Scan> Lexer::take_exponent(std::string& spelling, Place start) {
  if (index_ >= source_.size()) return std::nullopt;
  const SourceChar mark = peek_char();
  if (!mark.valid || (mark.code != U'e' && mark.code != U'E')) return std::nullopt;
  std::size_t look = index_ + mark.bytes;
  char sign = 0;
  if (look < source_.size()) {
    const SourceChar maybe = peek_char_at(look);
    if (maybe.valid && (maybe.code == U'+' || maybe.code == U'-')) {
      sign = static_cast<char>(maybe.code);
      look += maybe.bytes;
    }
  }
  const SourceChar digit = peek_char_at(look);
  // 指数記号やその符号の隣の `_` も桁の間ではない。指数を捨てて名前にしない。
  if (digit.valid && digit.code == U'_') {
    spelling.push_back(static_cast<char>(mark.code));
    if (sign != 0) spelling.push_back(sign);
    spelling.push_back('_');
    return fail_at(SyntaxReason::UnexpectedToken, spelling, start);
  }
  if (!digit.valid || digit_value(digit.code, 10) < 0) return std::nullopt;
  advance(mark.code, mark.bytes);
  spelling.push_back(static_cast<char>(mark.code));
  if (sign != 0) {
    const SourceChar signed_mark = peek_char();
    advance(signed_mark.code, signed_mark.bytes);
    spelling.push_back(sign);
  }
  const Digits exp = read_digits(10);
  spelling += exp.spelling;
  if (exp.bad_separator || !exp.any) {
    return fail_at(SyntaxReason::UnexpectedToken, spelling, start);
  }
  return std::nullopt;
}

Scan Lexer::make_float(const std::string& spelling, Place start) {
  std::string clean;
  clean.reserve(spelling.size());
  for (const char ch : spelling) {
    if (ch != '_') clean.push_back(ch);
  }
  double value = 0;
  const char* const first = clean.data();
  const char* const last = first + clean.size();
  const std::from_chars_result parsed = std::from_chars(first, last, value);
  if (parsed.ec == std::errc::result_out_of_range) {
    // 上位の溢れを inf にすると float_overflow=error のもとで評価されずに残り、inf.0 は読み戻せない（レビュー M2、
    // iso-notes 6.4.5 の A）。下位の溢れは今までどおり 0.0 にする。
    if (above_double(clean)) return fail_at(SyntaxReason::FloatOverflow, spelling, start);
    value = 0;
  } else if (parsed.ec != std::errc{} || parsed.ptr != last) {
    return fail_at(SyntaxReason::UnexpectedToken, spelling, start);
  }
  Scan scan = token_at(TokenKind::Float, start);
  scan.token.floating = value;
  return scan;
}

Scan Lexer::scan_quoted() {
  const Place start = here();
  const SourceChar opener = peek_char();
  // 「 は 」で閉じ、中は ' と同じ規則（iso-notes 第4節「…」の決定 A）。
  const char32_t quote = opener.code == U'「' ? U'」' : opener.code;
  std::string opener_text;
  append_code(opener_text, opener.code);
  advance(opener.code, opener.bytes);
  const FlagScope raw(raw_, true);
  std::string text;
  while (index_ < source_.size()) {
    const SourceChar ch = peek_char();
    if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
    if (ch.code == quote) {
      const SourceChar doubled = peek_char_at(index_ + ch.bytes);
      if (doubled.valid && doubled.code == quote) {
        append_code(text, quote);
        advance(ch.code, ch.bytes);
        advance(doubled.code, doubled.bytes);
        continue;
      }
      advance(ch.code, ch.bytes);
      const TokenKind kind = quote == U'\'' || quote == U'」' ? TokenKind::Name : TokenKind::String;
      return token_at(kind, start, std::move(text));
    }
    if (ch.code == U'\\') {
      if (const std::optional<Scan> error = append_escape(&text, nullptr)) return *error;
      continue;
    }
    // 行継続はバックスラッシュ側で消す。ここはバックスラッシュのない改行とタブだけ。
    if (raw_quote_break(ch.code)) {
      // トークンはその字そのもの。名前を付けるのは表示の側（messages_*.pl）。
      return fail_at(SyntaxReason::IllegalCharacter, ch.code == U'\t' ? "\t" : "\n", here());
    }
    append_code(text, ch.code);
    advance(ch.code, ch.bytes);
  }
  return fail_at(SyntaxReason::UnterminatedQuote, opener_text, start);
}

std::optional<Scan> Lexer::append_escape(std::string* text, char32_t* code) {
  const Place start = here();
  const SourceChar slash = peek_char();
  advance(slash.code, slash.bytes);
  if (index_ >= source_.size()) return fail_at(SyntaxReason::BadEscape, "\\", start);
  const SourceChar next = peek_char();
  if (!next.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
  if (next.code == U'\n' || next.code == U'\r') {
    // 0' は 1 文字が要る。行継続は文字を残さないので、文字コードでは不正。
    if (code != nullptr) return fail_at(SyntaxReason::BadEscape, "\\", start);
    return escape_continuation();
  }
  if (const std::optional<char32_t> simple = symbolic_escape(next.code)) {
    advance(next.code, next.bytes);
    emit_code(text, code, *simple);
    return std::nullopt;
  }
  if (next.code == U'x') return escape_hex_run(text, code, start);
  if (next.code == U'u') return escape_hex_exact(text, code, start, 4);
  if (next.code == U'U') return escape_hex_exact(text, code, start, 8);
  if (digit_value(next.code, 8) >= 0) return escape_octal(text, code, start);
  const CharClass kind = classify(next.code);
  if (kind == CharClass::Graphic || kind == CharClass::Solo) {
    advance(next.code, next.bytes);
    emit_code(text, code, next.code);
    return std::nullopt;
  }
  return fail_at(SyntaxReason::BadEscape, "\\", start);
}

std::optional<Scan> Lexer::escape_continuation() {
  const SourceChar br = peek_char();
  // 8.16.1.4 はバックスラッシュと改行だけを消す。直後の空白はアトムに残る。
  advance(br.code, br.bytes);
  return std::nullopt;
}

std::optional<Scan> Lexer::take_hex_digit(char32_t& value, Place start) {
  if (index_ >= source_.size()) return fail_at(SyntaxReason::BadEscape, "\\", start);
  const SourceChar ch = peek_char();
  if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
  const int digit = digit_value(ch.code, 16);
  if (digit < 0) return fail_at(SyntaxReason::BadEscape, "\\", start);
  const auto unit = static_cast<char32_t>(digit);
  if (value > (char32_t{0x10FFFF} - unit) / 16) {
    return fail_at(SyntaxReason::BadEscape, "\\", start);
  }
  value = static_cast<char32_t>(value * 16 + unit);
  advance(ch.code, ch.bytes);
  return std::nullopt;
}

std::optional<Scan> Lexer::escape_hex_exact(std::string* text, char32_t* code, Place start, int count) {
  const SourceChar marker = peek_char();
  advance(marker.code, marker.bytes);
  char32_t value = 0;
  for (int i = 0; i < count; ++i) {
    if (const std::optional<Scan> error = take_hex_digit(value, start)) return *error;
  }
  if (!code_point_ok(value)) return fail_at(SyntaxReason::BadEscape, "\\", start);
  emit_code(text, code, value);
  return std::nullopt;
}

std::optional<Scan> Lexer::escape_hex_run(std::string* text, char32_t* code, Place start) {
  const SourceChar marker = peek_char();
  advance(marker.code, marker.bytes);
  char32_t value = 0;
  bool any = false;
  while (index_ < source_.size()) {
    const SourceChar ch = peek_char();
    if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
    if (ch.code == U'\\') {
      if (!any || !code_point_ok(value)) return fail_at(SyntaxReason::BadEscape, "\\", start);
      advance(ch.code, ch.bytes);
      emit_code(text, code, value);
      return std::nullopt;
    }
    if (const std::optional<Scan> error = take_hex_digit(value, start)) return *error;
    any = true;
  }
  return fail_at(SyntaxReason::BadEscape, "\\", start);
}

std::optional<Scan> Lexer::escape_octal(std::string* text, char32_t* code, Place start) {
  char32_t value = 0;
  int count = 0;
  while (count < 3 && index_ < source_.size()) {
    const SourceChar ch = peek_char();
    if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
    const int digit = digit_value(ch.code, 8);
    if (digit < 0) break;
    value = static_cast<char32_t>(value * 8 + static_cast<char32_t>(digit));
    advance(ch.code, ch.bytes);
    ++count;
  }
  if (count == 0 || index_ >= source_.size()) return fail_at(SyntaxReason::BadEscape, "\\", start);
  const SourceChar tail = peek_char();
  if (!tail.valid || tail.code != U'\\') return fail_at(SyntaxReason::BadEscape, "\\", start);
  advance(tail.code, tail.bytes);
  emit_code(text, code, value);
  return std::nullopt;
}

void Lexer::emit_code(std::string* text, char32_t* code, char32_t value) {
  if (code != nullptr) *code = value;
  if (text != nullptr) append_code(*text, value);
}

}  // namespace mosa
