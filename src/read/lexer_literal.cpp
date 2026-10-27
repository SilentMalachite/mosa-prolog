#include "mosa/lexer.hpp"

#include "mosa/char_class.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
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

// 乗算の前に int64 の上限を見る。超えた整数は浮動小数にしない。
bool push_digit(std::int64_t& value, int base, int digit) {
  const auto limit = std::numeric_limits<std::int64_t>::max();
  if (value > (limit - digit) / base) return false;
  value = value * static_cast<std::int64_t>(base) + digit;
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
  Scan scan = token_at(TokenKind::Integer, start);
  scan.token.integer = whole.value;
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
  Scan scan = token_at(TokenKind::Integer, start);
  scan.token.integer = digits.value;
  return scan;
}

Scan Lexer::scan_char_code(Place start) {
  advance(U'0', 1);
  advance(U'\'', 1);
  if (index_ >= source_.size()) return fail_at(SyntaxReason::UnexpectedToken, "0'", start);
  const SourceChar ch = peek_char();
  if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
  char32_t value = 0;
  if (ch.code == U'\\') {
    if (const std::optional<Scan> error = append_escape(nullptr, &value)) return *error;
  } else if (ch.code == U'\n' || ch.code == U'\r') {
    return fail_at(SyntaxReason::UnexpectedToken, "0'", start);
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
    const bool tiny = clean.find('-') != std::string::npos;
    value = tiny ? 0 : std::numeric_limits<double>::infinity();
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
  const char32_t quote = opener.code;
  const std::string opener_text(1, static_cast<char>(quote));
  advance(quote, opener.bytes);
  const RawScope raw(*this);
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
      const TokenKind kind = quote == U'\'' ? TokenKind::Name : TokenKind::String;
      return token_at(kind, start, std::move(text));
    }
    if (ch.code == U'\\') {
      if (const std::optional<Scan> error = append_escape(&text, nullptr)) return *error;
      continue;
    }
    // 行継続はバックスラッシュ側で消す。ここはバックスラッシュのない改行とタブだけ。
    if (raw_quote_break(ch.code)) {
      return fail_at(SyntaxReason::IllegalCharacter, ch.code == U'\t' ? "タブ" : "改行", here());
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
