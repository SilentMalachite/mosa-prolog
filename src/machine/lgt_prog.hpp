#pragma once

// lgt_exec が動かす本体の木と、それを作る lgt_build の境目。テストランナーの外には出さない。
// 木を作る側を lgt_exec.cpp から分けた（800 行を超える前に、レビュー 2026-11-20 の計画）。

#include "lgt_priv.hpp"

#include <string>
#include <vector>

namespace mosa {

enum class LgtNodeKind { Plain, Conj, Disj, If, Soft, Pick, Assert, Variant, Subsumes, Approx, Forall, Super };

struct LgtNode {
  LgtNodeKind kind = LgtNodeKind::Plain;
  std::string text;
  std::string note;
  int left = -1;
  int right = -1;
  // Soft と Pick の else。If だけでは else を持たない。
  int other = -1;
  // Pick（else にだけランナー目標がある (C -> T ; E)）。text は印の変数 marker を付けた (C -> (T, M = then) ; M = else)、
  // arrow は (C -> T)、cond は C の字面。開いた query があるときは arrow と cond をスクラッチで試す（lgt_exec.cpp）。
  std::string marker;
  std::string arrow;
  std::string cond;
};

struct LgtProg {
  std::vector<LgtNode> nodes;
};

// 本体の項を、ランナーが判定する目標で分けた木にし、根の添字を返す（lgt_build.cpp）。
[[nodiscard]] int lgt_build_node(LgtProg& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                                 const std::vector<VarName>& names);

}  // namespace mosa
