#pragma once

// 出現検査はしない。選択点スタックは持たない。失敗したときだけ束縛とヒープ長を戻す。

#include "mosa/heap.hpp"

namespace mosa {

[[nodiscard]] bool unify(Heap& heap, Cell left, Cell right);

}
