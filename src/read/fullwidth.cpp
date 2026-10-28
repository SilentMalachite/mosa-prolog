// SPEC 第4節 全角記号モード（旗 japanese_punctuation）。iso-notes「Section 4 Flag japanese_punctuation」。
// 引数 1 字。誤りは投げない。試験は tests/cpp/lexer_test.cpp と tests/pl/japanese.pl の fw_*。
#include "mosa/char_class.hpp"

#include <optional>

namespace mosa {

std::optional<char32_t> halfwidth_punctuation(char32_t code_point) {
  switch (code_point) {
    case U'（':
      return U'(';
    case U'）':
      return U')';
    case U'［':
      return U'[';
    case U'］':
      return U']';
    // 読点も引数の区切り。文の区切りの役は持たない。
    case U'，':
    case U'、':
      return U',';
    case U'：':
      return U':';
    default:
      return std::nullopt;
  }
}

}  // namespace mosa
