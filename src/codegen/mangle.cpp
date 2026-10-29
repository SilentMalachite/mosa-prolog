// 計画 Task 4。Prolog の名前を C++ の識別子の一部にする。C++ は __ を含む識別子を予約している
// （[lex.name]）ので、出す識別子に __ を作らない（持ち主の決定 B、2026-10-29）。
// _ は _5f にし、ほかの非英数字のバイトは _ と 2 桁の小文字 16 進にする。出力の _ は必ず直後の
// 2 桁の 16 進とひと組なので、左から読めば元のバイト列に戻せる（単射）。_ が続く形（__）は出ない。

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
      out += "_5f";
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
