#pragma once

// 節は継続を最後に持つ木にする。実行ループはこの木をゴール枠にしてから呼ぶ。

#include <cstdint>
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
  Halt
};

struct IrTerm {
  IrTag tag = IrTag::True;
  std::uint16_t slot = 0;
  std::uint32_t atom = 0;
  std::uint16_t arity = 0;
  std::int64_t integer = 0;
  double floating = 0;
  std::vector<IrTerm> args;
};

}
