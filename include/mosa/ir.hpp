#pragma once

// 節は継続を最後に持つ木にする。実行ループはこの木をゴール枠にしてから呼ぶ。

#include <cstdint>
#include <span>
#include <vector>

namespace mosa {

enum class IrTag {
  Var,
  Anon,
  Atom,
  Int,
  Float,
  List,
  Fun,
  True,
  Fail,
  Cut,
  Call,
  And,
  Or,
  IfThen,
  SoftIf,
  Catch,
  Throw,
  Halt,
  // 整数の Big（ADR 0025）。integer は負なら 1、args は絶対値の語を下位から持つ Int の子（integer が語のビット列）。
  // 節の指紋（native.cpp）は札の番号を数えるので、新しい札は最後に足す。
  Big
};

// 数の葉。ゴールにならない（7.6.2）ので、呼び出せる項の判定がそろって使う。
[[nodiscard]] constexpr bool is_number_ir(IrTag tag) {
  return tag == IrTag::Int || tag == IrTag::Float || tag == IrTag::Big;
}

// 破棄と複製は、深さ kShallow（src/core/ir.cpp）までは C++ の再帰で、それより深い所は手で積んで行う
// （ADR 0011）。長いリストは args[1] の入れ子になる。節の本体は呼び出しごとに写して壊す（step_control と
// embody）ので、浅い項の手間を既定の特殊メンバ関数に近く保つ。
struct IrTerm {
  IrTag tag = IrTag::True;
  std::uint32_t slot = 0;
  std::uint32_t atom = 0;
  std::uint16_t arity = 0;
  std::int64_t integer = 0;
  double floating = 0;
  std::vector<IrTerm> args;

  // 複製の深さ。C++ の呼び出しとして重なった複製の数を運ぶ。
  struct Depth {
    int value = 0;
  };

  IrTerm() = default;
  IrTerm(const IrTerm& other) : IrTerm(other, Depth{}) {}
  IrTerm(const IrTerm& other, Depth depth)
      : tag(other.tag), slot(other.slot), atom(other.atom), arity(other.arity), integer(other.integer),
        floating(other.floating) {
    if (!other.args.empty()) copy_args(other, depth);
  }
  IrTerm(IrTerm&& other) noexcept = default;
  IrTerm& operator=(const IrTerm& other);
  // 自分の子からのムーブ（term = std::move(term.args[1])）も受ける。
  IrTerm& operator=(IrTerm&& other) noexcept;
  ~IrTerm() {
    if (!args.empty()) release(0);
  }

 private:
  void copy_args(const IrTerm& other, Depth depth);
  void release(int depth) noexcept;
};

// IrTag::Big の節点と絶対値の語の行き来（src/core/ir.cpp）。語は上位の 0 を持たない前提で、そのまま写す。
[[nodiscard]] IrTerm big_ir(bool negative, std::span<const std::uint64_t> magnitude);
[[nodiscard]] std::vector<std::uint64_t> big_words(const IrTerm& term);

}
