#pragma once

// デバッガのポート（ADR 0028）。ポートを見せるか、止まってコマンドを読むかを 1 か所で決める。箱の出入り
// （debug_box.cpp）が呼ぶ。

#include "internal.hpp"

#include <cstddef>
#include <cstdint>

namespace mosa {

// stack[box_index] の番兵のポートの行を DebugConsole::write に書き、止まるポートなら 1 行読んでコマンドを当てる。ball は
// 例外のポートで投げられた項、ほかのポートでは nullptr。口が無いか、デバッグが切りか、進み方で見せないポートなら、
// 何も書かずに Go。PortAction は debug_state.hpp。
[[nodiscard]] PortAction debug_port(Impl& machine, DebugPort port, std::size_t box_index, const Cell* ball);

}  // namespace mosa
