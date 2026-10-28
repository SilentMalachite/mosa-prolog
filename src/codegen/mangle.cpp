// 計画 Task 4。Prolog の名前を C++ の識別子の一部にする。_ を __ にするので、_ と 16 進の並びは
// 元の名前の _ と取り違えない（_ の直後が _ か 16 進かで分かれる）。

#include "codegen.hpp"

#include <string>
#include <string_view>

namespace mosa::codegen {
namespace {

// <cctype> の isalnum はロケールで変わる。識別子に使える ASCII の英数字だけを残す。
bool plain(char unit) {
  return (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9');
}

}  // namespace

std::string mangle(std::string_view name) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string out;
  out.reserve(name.size());
  for (const char unit : name) {
    if (plain(unit)) {
      out += unit;
    } else if (unit == '_') {
      out += "__";
    } else {
      const auto byte = static_cast<unsigned char>(unit);
      out += '_';
      out += digits[byte >> 4];
      out += digits[byte & 0x0F];
    }
  }
  return out;
}

}  // namespace mosa::codegen
