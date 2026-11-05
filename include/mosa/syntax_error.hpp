#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace mosa {

// コメント未終了を引用や不正トークンと混ぜない。token は "/*" に固定できる。
enum class SyntaxReason {
  InvalidUtf8,
  IllegalCharacter,
  UnterminatedQuote,
  UnterminatedComment,
  BadEscape,
  IntegerOverflow,
  FloatOverflow,
  UnexpectedToken,
  MaxArity,
  IncompleteTerm,
};

// syntax_error(Key) の Key（ADR 0007）。言語に依らない英語の原子で、文面は messages_*.pl の同じ鍵が持つ。
[[nodiscard]] constexpr std::string_view syntax_key(SyntaxReason reason) {
  switch (reason) {
    case SyntaxReason::InvalidUtf8: return "invalid_utf8";
    case SyntaxReason::IllegalCharacter: return "illegal_character";
    case SyntaxReason::UnterminatedQuote: return "unterminated_quoted";
    case SyntaxReason::UnterminatedComment: return "unterminated_comment";
    case SyntaxReason::BadEscape: return "bad_escape";
    case SyntaxReason::IntegerOverflow: return "integer_overflow";
    case SyntaxReason::FloatOverflow: return "float_overflow";
    case SyntaxReason::UnexpectedToken: return "unexpected_token";
    case SyntaxReason::MaxArity: return "max_arity";
    case SyntaxReason::IncompleteTerm: return "incomplete_term";
  }
  return "unexpected_token";
}

struct SyntaxFailure {
  std::size_t line = 1;
  std::size_t column = 1;
  std::string token;
  SyntaxReason reason = SyntaxReason::IllegalCharacter;
  // 失敗した所の、読んだ字の先頭からのバイト位置。char_conversion は行と列を変えるが、これは変えない。壊れた UTF-8 を
  // 置き換える位置を行と列から求めると、改行を変換したときずれた（L6 の Codex レビュー）。
  std::size_t byte = 0;
};

}
