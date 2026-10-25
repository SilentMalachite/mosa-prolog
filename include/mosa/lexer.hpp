#pragma once

#include "mosa/char_conv.hpp"
#include "mosa/syntax_error.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mosa {

enum class TokenKind { End, Name, Variable, Integer, Float, String, Solo, EndFullStop };

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
  std::int64_t integer = 0;
  double floating = 0;
  std::size_t line = 1;
  std::size_t column = 1;
  std::size_t byte = 0;
  // 空白や注釈の直後の '(' は関数表記ではない。
  bool after_layout = false;
};

struct Scan {
  Token token;
  std::optional<SyntaxFailure> failure;
};

// source は最後の next が返るまで有効であればよい。テキストは展開後を所有する。
class Lexer {
 public:
  explicit Lexer(std::string_view source, const CharConv* conv = nullptr);
  [[nodiscard]] Scan next();
  // 終止符は、続く空白 1 つまで含めたバイト位置。
  [[nodiscard]] std::size_t offset() const { return index_; }

 private:
  struct SourceChar {
    char32_t code = 0;
    std::size_t bytes = 0;
    bool valid = false;
  };

  struct Place {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t byte = 0;
  };

  struct Digits {
    std::string spelling;
    std::int64_t value = 0;
    bool overflow = false;
    bool bad_separator = false;
    bool any = false;
  };

  [[nodiscard]] Place here() const;
  // 引用と注釈の間だけ、写像せずに字を読む。
  class RawScope {
   public:
    explicit RawScope(Lexer& lexer) : lexer_(lexer), previous_(lexer.raw_) { lexer_.raw_ = true; }
    ~RawScope() { lexer_.raw_ = previous_; }
    RawScope(const RawScope&) = delete;
    RawScope& operator=(const RawScope&) = delete;

   private:
    Lexer& lexer_;
    bool previous_;
  };

  [[nodiscard]] SourceChar peek_char() const;
  [[nodiscard]] SourceChar peek_raw_at(std::size_t byte) const;
  [[nodiscard]] SourceChar peek_char_at(std::size_t byte) const;
  void advance(char32_t code, std::size_t bytes);
  void append_code(std::string& text, char32_t code) const;
  void skip_bom();
  [[nodiscard]] std::optional<Scan> skip_ignorable();
  [[nodiscard]] std::optional<Scan> skip_block_comment(Place start);
  [[nodiscard]] std::optional<Scan> skip_line_comment();
  [[nodiscard]] bool dot_is_full_stop() const;
  [[nodiscard]] Scan scan_word(TokenKind kind);
  [[nodiscard]] Scan scan_graphic();
  [[nodiscard]] Scan scan_solo();
  [[nodiscard]] Scan scan_number();
  // nullopt は小数点の隣の `_`。false は小数部なし、true は桁が続く。
  [[nodiscard]] std::optional<bool> fraction_follows() const;
  [[nodiscard]] bool radix_ready(int base) const;
  [[nodiscard]] Scan scan_char_code(Place start);
  [[nodiscard]] Scan scan_radix(int base, char prefix, Place start);
  [[nodiscard]] Scan scan_decimal(Place start);
  [[nodiscard]] Digits read_digits(int base);
  [[nodiscard]] Scan finish_float(std::string spelling, Place start);
  [[nodiscard]] std::optional<Scan> take_exponent(std::string& spelling, Place start);
  [[nodiscard]] Scan make_float(const std::string& spelling, Place start);
  [[nodiscard]] Scan scan_quoted();
  [[nodiscard]] std::optional<Scan> append_escape(std::string* text, char32_t* code);
  [[nodiscard]] std::optional<Scan> escape_continuation();
  [[nodiscard]] std::optional<Scan> escape_hex_exact(std::string* text, char32_t* code, Place start,
                                                     int count);
  [[nodiscard]] std::optional<Scan> escape_hex_run(std::string* text, char32_t* code, Place start);
  [[nodiscard]] std::optional<Scan> escape_octal(std::string* text, char32_t* code, Place start);
  [[nodiscard]] std::optional<Scan> take_hex_digit(char32_t& value, Place start);
  void emit_code(std::string* text, char32_t* code, char32_t value);
  [[nodiscard]] Scan fail_at(SyntaxReason reason, std::string token, Place place);
  [[nodiscard]] Scan token_at(TokenKind kind, Place place, std::string text = {});

  std::string_view source_;
  std::size_t index_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  bool bom_checked_ = false;
  bool layout_before_ = false;
  bool raw_ = false;
  const CharConv* conv_ = nullptr;
  std::optional<Scan> failure_;
};

}
