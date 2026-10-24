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

}
