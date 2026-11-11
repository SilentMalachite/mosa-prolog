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
  // 大きさがちょうど 2^63 の整数。- の後ろでだけ負の定数 min_integer になり（6.3.4.1、レビュー M1）、integer は
  // そのとき使う値 min_integer を持つ。ほかの位置では読む側が integer_overflow にする。
  bool needs_minus = false;
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
  // 終止符の位置はドットの直後。確認した空白は含めない。
  [[nodiscard]] std::size_t offset() const { return index_; }
  // 張り付いた失敗を外す。位置が動いていなければ 1 字捨てる。
  // 捨てないと次の next が同じ失敗を返し、終止符まで届かない。
  bool release_failure();
  // 旗 japanese_punctuation。読み込みでは指令の次の節から効く。
  void set_fullwidth(bool on) { fullwidth_ = on; }
  // source の最後の字が終止符の後ろの字（layout char か %）で、その前の字が終止符になりうるとき真。写像は next と
  // 同じく conv と全角の旗で決める。標準入力の read が、足した字ごとでなく、ここが真のときだけ読み直すために使う。
  // 引用や注釈の中かは見ないので真は多めに出るが、終止符で止まる所では必ず真になる（6.4.8）。
  [[nodiscard]] static bool may_end_before_last(std::string_view source, const CharConv* conv, bool fullwidth);
  // 標準入力の read の下見の続き。from から読めば、頭から読んだときと同じ字句が続く。inside が None でなければ、
  // from は入力の終わりで切れた引用（閉じる引用符は quote）か注釈の中の、最後に読んだ字か escape の始まりで、
  // そこから中身として読み続ける。最後の字か escape を読み直すのは、\ と \r の行継続が次の \n も取るため。
  enum class Inside : std::uint8_t { None, Quote, Block, Line };
  struct Scout {
    std::size_t from = 0;
    Inside inside = Inside::None;
    char32_t quote = 0;
  };
  // may_end_before_last が真の source を state.from から字句だけ読み、読み手が止まりうる所（終止符か、終わりで
  // 切れていない失敗）があれば真。偽なら state を終わりで切れた所へ進める。終止符らしい並びのたびに頭から読み直すと、
  // 引用や注釈の中にその並びが多い長い項で、時間が長さの 2 乗に戻った（stdin_scale_test、Muse のレビュー）。
  [[nodiscard]] static bool scout(std::string_view source, const CharConv* conv, bool fullwidth, Scout& state);

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
    // 大きさは 2^63 まで数える。符号を付けると min_integer になる 2^63 を、溢れと分けるため。
    std::uint64_t value = 0;
    bool overflow = false;
    bool bad_separator = false;
    bool any = false;
  };

  [[nodiscard]] Place here() const;
  // 引用と注釈の間は写像せずに字を読む。0'c の間は全角を半角にしない。
  class FlagScope {
   public:
    FlagScope(bool& flag, bool value) : flag_(flag), previous_(flag) { flag_ = value; }
    ~FlagScope() { flag_ = previous_; }
    FlagScope(const FlagScope&) = delete;
    FlagScope& operator=(const FlagScope&) = delete;

   private:
    bool& flag_;
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
  [[nodiscard]] bool end_follows(std::size_t width) const;
  [[nodiscard]] bool ends_clause(SourceChar ch) const;
  [[nodiscard]] Scan scan_word(TokenKind kind);
  [[nodiscard]] Scan scan_graphic();
  [[nodiscard]] Scan scan_solo();
  [[nodiscard]] Scan scan_number();
  // nullopt は小数点の隣の `_`。false は小数部なし、true は桁が続く。
  [[nodiscard]] std::optional<bool> fraction_follows() const;
  [[nodiscard]] bool radix_ready(int base) const;
  [[nodiscard]] Scan scan_char_code(Place start);
  [[nodiscard]] Scan scan_radix(int base, char prefix, Place start);
  [[nodiscard]] Scan integer_token(std::uint64_t magnitude, std::string spelling, Place start);
  [[nodiscard]] Scan scan_decimal(Place start);
  [[nodiscard]] Digits read_digits(int base);
  [[nodiscard]] Scan finish_float(std::string spelling, Place start);
  [[nodiscard]] std::optional<Scan> take_exponent(std::string& spelling, Place start);
  [[nodiscard]] std::optional<Scan> take_special(const std::string& spelling, Place start);
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
  [[nodiscard]] bool skip_one();
  [[nodiscard]] std::optional<Scan> resume(const Scout& state);
  [[nodiscard]] bool past_failure(Scout& state);
  [[nodiscard]] Scan quoted_body(char32_t quote, Place start, const std::string& opener_text);
  [[nodiscard]] std::optional<Scan> block_body(Place start);
  [[nodiscard]] std::optional<Scan> line_body();

  std::string_view source_;
  std::size_t index_ = 0;
  // scout が、終わりで切れた所から読み続けるための印。item_ は読んでいる空白か注釈かトークンの始まり、unit_ は
  // 引用か注釈の中で最後に読み始めた字か escape の始まり。cut_ は中身を読む途中で終わりに当たった引用か注釈。
  std::size_t item_ = 0;
  std::size_t unit_ = 0;
  Inside cut_ = Inside::None;
  char32_t cut_quote_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  bool bom_checked_ = false;
  bool layout_before_ = false;
  bool raw_ = false;
  bool fullwidth_ = false;
  const CharConv* conv_ = nullptr;
  std::optional<Scan> failure_;
  std::size_t attempt_ = 0;
  bool failure_stuck_ = false;
};

}
