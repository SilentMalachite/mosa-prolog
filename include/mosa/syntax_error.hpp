#pragma once

#include <cstddef>
#include <string>

namespace mosa {

// コメント未終了を引用や不正トークンと混ぜない。token は "/*" に固定できる。
enum class SyntaxReason {
  InvalidUtf8,
  IllegalCharacter,
  UnterminatedQuote,
  UnterminatedComment,
  BadEscape,
  IntegerOverflow,
  UnexpectedToken,
  MaxArity,
  IncompleteTerm,
};

struct SyntaxFailure {
  std::size_t line = 1;
  std::size_t column = 1;
  std::string token;
  SyntaxReason reason = SyntaxReason::IllegalCharacter;
};

}
