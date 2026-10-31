#include "mosa/lexer.hpp"

#include "mosa/char_class.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mosa {
namespace {

struct Decoded {
  char32_t code = 0;
  std::size_t bytes = 0;
  bool valid = false;
};

int utf8_continuation(std::string_view source, std::size_t index) {
  if (index >= source.size()) return -1;
  const auto byte = static_cast<unsigned char>(source[index]);
  if ((byte & 0xC0U) != 0x80U) return -1;
  return static_cast<int>(byte & 0x3FU);
}

Decoded decoded_ok(char32_t code, std::size_t bytes) { return Decoded{code, bytes, true}; }

Decoded decode_two(unsigned char lead, std::string_view source, std::size_t index) {
  const int next = utf8_continuation(source, index + 1);
  if (next < 0) return {};
  const auto code = static_cast<char32_t>((lead & 0x1FU) << 6) | static_cast<char32_t>(next);
  return decoded_ok(code, 2);
}

// 過剰な 3 バイト列とサロゲートは Unicode スカラー値ではない。
Decoded decode_three(unsigned char lead, std::string_view source, std::size_t index) {
  const int c1 = utf8_continuation(source, index + 1);
  const int c2 = utf8_continuation(source, index + 2);
  if (c1 < 0 || c2 < 0) return {};
  if (lead == 0xE0U && c1 < 0x20) return {};
  if (lead == 0xEDU && c1 >= 0x20) return {};
  const auto code = static_cast<char32_t>((lead & 0x0FU) << 12) | (static_cast<char32_t>(c1) << 6) |
                    static_cast<char32_t>(c2);
  return decoded_ok(code, 3);
}

Decoded decode_four(unsigned char lead, std::string_view source, std::size_t index) {
  const int c1 = utf8_continuation(source, index + 1);
  const int c2 = utf8_continuation(source, index + 2);
  const int c3 = utf8_continuation(source, index + 3);
  if (c1 < 0 || c2 < 0 || c3 < 0) return {};
  if (lead == 0xF0U && c1 < 0x10) return {};
  if (lead == 0xF4U && c1 >= 0x10) return {};
  const auto code = (static_cast<char32_t>(lead & 0x07U) << 18) | (static_cast<char32_t>(c1) << 12) |
                    (static_cast<char32_t>(c2) << 6) | static_cast<char32_t>(c3);
  if (code > 0x10FFFFU) return {};
  return decoded_ok(code, 4);
}

Decoded decode_utf8(std::string_view source, std::size_t index) {
  if (index >= source.size()) return {};
  const auto lead = static_cast<unsigned char>(source[index]);
  if (lead <= 0x7FU) return decoded_ok(lead, 1);
  if (lead >= 0xC2U && lead <= 0xDFU) return decode_two(lead, source, index);
  if (lead >= 0xE0U && lead <= 0xEFU) return decode_three(lead, source, index);
  if (lead >= 0xF0U && lead <= 0xF4U) return decode_four(lead, source, index);
  return {};
}

}  // namespace

Lexer::Lexer(std::string_view source, const CharConv* conv) : source_(source), conv_(conv) {}

Scan Lexer::next() {
  // 失敗を残す。不正バイトを消費した次の next も同じ失敗を返すため。
  if (failure_) return *failure_;
  skip_bom();
  attempt_ = index_;
  const std::size_t before = index_;
  if (const std::optional<Scan> stopped = skip_ignorable()) return *stopped;
  // 名前と '(' の間の空白は、関数表記と前置演算子の引数を分ける。
  layout_before_ = index_ != before;
  if (index_ >= source_.size()) return token_at(TokenKind::End, here());
  const SourceChar ch = peek_char();
  if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
  if (fullwidth_ && ch.code == U'「') return scan_quoted();
  switch (classify(ch.code)) {
    case CharClass::Small:
      return scan_word(TokenKind::Name);
    case CharClass::Capital:
    case CharClass::Underline:
      return scan_word(TokenKind::Variable);
    case CharClass::Digit:
      return scan_number();
    case CharClass::Graphic:
      return scan_graphic();
    case CharClass::Solo:
      return scan_solo();
    case CharClass::Quote:
      return scan_quoted();
    case CharClass::Layout:
    case CharClass::Mark:
    case CharClass::Illegal:
      break;
  }
  return fail_at(SyntaxReason::IllegalCharacter, std::string(source_.substr(index_, ch.bytes)), here());
}

Lexer::Place Lexer::here() const { return Place{line_, column_, index_}; }

Lexer::SourceChar Lexer::peek_char() const { return peek_char_at(index_); }

Lexer::SourceChar Lexer::peek_raw_at(std::size_t byte) const {
  const Decoded decoded = decode_utf8(source_, byte);
  return SourceChar{decoded.code, decoded.bytes, decoded.valid};
}

Lexer::SourceChar Lexer::peek_char_at(std::size_t byte) const {
  SourceChar ch = peek_raw_at(byte);
  // 引用と注釈の中は元の文字。外は写像した符号点で分類する。
  // バイト数は元の文字のままにして、消費位置をソースに合わせる。
  if (!ch.valid || raw_) return ch;
  if (conv_ != nullptr) ch.code = conv_->apply(ch.code);
  if (!fullwidth_) return ch;
  if (const std::optional<char32_t> half = halfwidth_punctuation(ch.code)) ch.code = *half;
  return ch;
}

void Lexer::advance(char32_t code, std::size_t bytes) {
  index_ += bytes;
  // CR LF は 2 バイトでも改行 1 つ。列は表示幅ではなくスカラー値の個数。
  if (code == U'\r') {
    if (index_ < source_.size() && source_[index_] == '\n') ++index_;
    ++line_;
    column_ = 1;
    return;
  }
  if (code == U'\n') {
    ++line_;
    column_ = 1;
    return;
  }
  ++column_;
}

void Lexer::append_code(std::string& text, char32_t code) const {
  const auto push = [&text](unsigned int byte) {
    text.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
  };
  if (code <= 0x7FU) {
    push(static_cast<unsigned int>(code));
    return;
  }
  if (code <= 0x7FFU) {
    push(0xC0U | static_cast<unsigned int>(code >> 6));
    push(0x80U | static_cast<unsigned int>(code & 0x3FU));
    return;
  }
  if (code <= 0xFFFFU) {
    push(0xE0U | static_cast<unsigned int>(code >> 12));
    push(0x80U | static_cast<unsigned int>((code >> 6) & 0x3FU));
    push(0x80U | static_cast<unsigned int>(code & 0x3FU));
    return;
  }
  push(0xF0U | static_cast<unsigned int>(code >> 18));
  push(0x80U | static_cast<unsigned int>((code >> 12) & 0x3FU));
  push(0x80U | static_cast<unsigned int>((code >> 6) & 0x3FU));
  push(0x80U | static_cast<unsigned int>(code & 0x3FU));
}

void Lexer::skip_bom() {
  if (bom_checked_) return;
  bom_checked_ = true;
  const SourceChar ch = peek_raw_at(0);
  // 先頭の BOM だけ捨てる。列 1 は最初の本文の文字に残す。
  if (index_ == 0 && ch.valid && ch.code == U'\uFEFF') index_ = ch.bytes;
}

std::optional<Scan> Lexer::skip_ignorable() {
  while (index_ < source_.size()) {
    const SourceChar ch = peek_char();
    if (!ch.valid) return std::nullopt;
    if (ch.code == U'/') {
      const SourceChar next = peek_char_at(index_ + ch.bytes);
      if (next.valid && next.code == U'*') {
        if (const std::optional<Scan> error = skip_block_comment(here())) return error;
        continue;
      }
    }
    if (ch.code == U'%') {
      if (const std::optional<Scan> error = skip_line_comment()) return error;
      continue;
    }
    if (classify(ch.code) != CharClass::Layout) return std::nullopt;
    advance(ch.code, ch.bytes);
  }
  return std::nullopt;
}

std::optional<Scan> Lexer::skip_block_comment(Place start) {
  const SourceChar slash = peek_char();
  advance(slash.code, slash.bytes);
  const SourceChar star = peek_char();
  advance(star.code, star.bytes);
  const FlagScope raw(raw_, true);
  while (index_ < source_.size()) {
    // ネストしない。最初の */ で閉じ、中の /* は文字のまま捨てる。
    if (source_[index_] == '*' && index_ + 1 < source_.size() && source_[index_ + 1] == '/') {
      advance(U'*', 1);
      advance(U'/', 1);
      return std::nullopt;
    }
    const SourceChar ch = peek_char();
    if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
    advance(ch.code, ch.bytes);
  }
  return fail_at(SyntaxReason::UnterminatedComment, "/*", start);
}

std::optional<Scan> Lexer::skip_line_comment() {
  const SourceChar mark = peek_char();
  advance(mark.code, mark.bytes);
  const FlagScope raw(raw_, true);
  while (index_ < source_.size()) {
    const auto byte = static_cast<unsigned char>(source_[index_]);
    if (byte == '\n' || byte == '\r') return std::nullopt;
    const SourceChar ch = peek_char();
    if (!ch.valid) return fail_at(SyntaxReason::InvalidUtf8, {}, here());
    advance(ch.code, ch.bytes);
  }
  return std::nullopt;
}

// 6.4.8: end char の後ろは layout char か %。% は注釈の開始なので、次の読みが飛ばす。
bool Lexer::end_follows(std::size_t width) const {
  if (index_ + width >= source_.size()) return true;
  const SourceChar after = peek_char_at(index_ + width);
  return after.valid && (classify(after.code) == CharClass::Layout || after.code == U'%');
}

bool Lexer::ends_clause(SourceChar ch, Place start) const {
  // 空白の前のドットは終止符。ただし直前もドットなら =.. の一部なので切らない。
  if (ch.code == U'.') {
    return end_follows(ch.bytes) && (index_ == start.byte || source_[index_ - 1] != '.');
  }
  // 。は小数点にしないので半角に写さない。後ろが空白か終わりのときだけ終止符で、他は記号文字。
  return fullwidth_ && ch.code == U'。' && end_follows(ch.bytes);
}

Scan Lexer::scan_word(TokenKind kind) {
  const Place start = here();
  std::string text;
  while (index_ < source_.size()) {
    const SourceChar ch = peek_char();
    if (!ch.valid) break;
    const CharClass cls = classify(ch.code);
    const bool continues = cls == CharClass::Small || cls == CharClass::Capital ||
                           cls == CharClass::Underline || cls == CharClass::Digit ||
                           cls == CharClass::Mark;
    if (!continues) break;
    append_code(text, ch.code);
    advance(ch.code, ch.bytes);
  }
  return token_at(kind, start, std::move(text));
}

Scan Lexer::scan_graphic() {
  const Place start = here();
  std::string text;
  while (index_ < source_.size()) {
    const SourceChar ch = peek_char();
    if (!ch.valid || classify(ch.code) != CharClass::Graphic || ends_clause(ch, start)) break;
    append_code(text, ch.code);
    advance(ch.code, ch.bytes);
  }
  if (index_ == start.byte) {
    const SourceChar stop = peek_char();
    advance(stop.code, stop.bytes);
    // 8.14.1.4 は終止符の直後の空白を次の項の先頭に残す。確認は先読みだけで足りる。
    return token_at(TokenKind::EndFullStop, start);
  }
  return token_at(TokenKind::Name, start, std::move(text));
}

Scan Lexer::scan_solo() {
  const Place start = here();
  const SourceChar ch = peek_char();
  std::string text;
  append_code(text, ch.code);
  advance(ch.code, ch.bytes);
  return token_at(TokenKind::Solo, start, std::move(text));
}

Scan Lexer::fail_at(SyntaxReason reason, std::string token, Place place) {
  Scan scan;
  scan.failure = SyntaxFailure{place.line, place.column, std::move(token), reason};
  // 桁などは失敗の前に進む。1 字も動いていない失敗は、同じ場所で繰り返す。
  failure_stuck_ = index_ == attempt_;
  failure_ = scan;
  return scan;
}

bool Lexer::skip_one() {
  if (index_ >= source_.size()) return false;
  const SourceChar ch = peek_char();
  if (!ch.valid || ch.bytes == 0) {
    ++index_;
    return true;
  }
  advance(ch.code, ch.bytes);
  return true;
}

bool Lexer::release_failure() {
  if (!failure_) return false;
  const bool stuck = failure_stuck_;
  failure_.reset();
  if (!stuck) return true;
  return skip_one();
}

Scan Lexer::token_at(TokenKind kind, Place place, std::string text) {
  Scan scan;
  scan.token.kind = kind;
  scan.token.text = std::move(text);
  scan.token.line = place.line;
  scan.token.column = place.column;
  scan.token.byte = place.byte;
  scan.token.after_layout = layout_before_;
  return scan;
}

}  // namespace mosa
