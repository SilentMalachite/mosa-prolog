#pragma once

// 出現検査はしない。失敗したときだけ、渡した印まで束縛とヒープ長を戻す。
// 成功したトレイルは残す。選択点より前の REF だけを積む。

#include "mosa/heap.hpp"

#include <cstdint>
#include <vector>

namespace mosa {

struct Trail {
  std::vector<std::uint64_t*> slots;
  // 選択点のヒープ水位。nullptr のときは、今回の呼び出し開始水位だけを境にする。
  const std::uint64_t* mark = nullptr;
};

[[nodiscard]] bool unify(Heap& heap, Trail& trail, std::size_t trail_mark, std::size_t heap_mark, Cell left,
                         Cell right);

}
