#pragma once

// 下位 3bit がタグ。タグ 0 は番地をそのまま書けるので、未束縛 REF は自分の番地を値にする。
// 関数語と Float / IntBox のペイロードは項ではない。ADR 0002。

#include <cstdint>

namespace mosa {

enum class Tag : std::uint64_t {
  Ref = 0,
  Atom = 1,
  Int = 2,
  Str = 3,
  Lis = 4,
  Float = 5,
  Big = 6,
  IntBox = 7,
};

class Cell {
 public:
  constexpr Cell() = default;
  explicit constexpr Cell(std::uint64_t raw) : raw_(raw) {}

  [[nodiscard]] constexpr Tag tag() const { return static_cast<Tag>(raw_ & 7U); }
  [[nodiscard]] constexpr std::uint64_t raw() const { return raw_; }

 private:
  std::uint64_t raw_ = 0;
};

// 引数順はアトム番号、アリティ。下位 3bit は 0、続く 11bit がアリティ、続く 32bit がアトム番号。
[[nodiscard]] constexpr std::uint64_t pack_functor(std::uint32_t atom, std::uint16_t arity) {
  return (static_cast<std::uint64_t>(atom) << 14) | (static_cast<std::uint64_t>(arity) << 3);
}

[[nodiscard]] constexpr std::uint16_t functor_arity(std::uint64_t word) {
  return static_cast<std::uint16_t>((word >> 3) & 0x7FFU);
}

[[nodiscard]] constexpr std::uint32_t functor_atom_id(std::uint64_t word) {
  return static_cast<std::uint32_t>(word >> 14);
}

// Big の箱の頭の語（ADR 0025）。関数語と同じく項ではない語なので（ADR 0002 の改訂 1）、下位 3bit を 0 にそろえる。
// bit 3 が符号、bit 4 から上が絶対値の語数。GC は語数を読むだけで箱の大きさが分かる。
[[nodiscard]] constexpr std::uint64_t big_header(std::uint64_t words, bool negative) {
  return (words << 4) | (static_cast<std::uint64_t>(negative) << 3);
}

[[nodiscard]] constexpr std::uint64_t big_header_words(std::uint64_t header) { return header >> 4; }

[[nodiscard]] constexpr bool big_header_negative(std::uint64_t header) { return ((header >> 3) & 1U) != 0; }

// 整数の値を持つタグ。int64 の即値と箱、その外の Big の 3 つ（正規形の規則で、同じ値が 2 つのタグに分かれない）。
[[nodiscard]] constexpr bool is_integer_tag(Tag tag) {
  return tag == Tag::Int || tag == Tag::IntBox || tag == Tag::Big;
}

// 頭の語の配置を書き換えて ADR 0025 からずれれば、ここでコンパイルが止まる。
static_assert(big_header(3, true) == 0x38U && (big_header(3, true) & 7U) == 0 &&
              big_header_words(big_header(3, true)) == 3 && big_header_negative(big_header(3, true)) &&
              !big_header_negative(big_header(2, false)) && is_integer_tag(Tag::Big) && !is_integer_tag(Tag::Float));

}
