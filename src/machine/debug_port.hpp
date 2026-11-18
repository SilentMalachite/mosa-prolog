#pragma once

// デバッガのポート（ADR 0028）。ポートを見せるか、止まってコマンドを読むかを 1 か所で決める。箱の出入り
// （debug_box.cpp）が呼ぶ。

#include "internal.hpp"

#include <cstddef>
#include <cstdint>

namespace mosa {

// 止まったポートで読んだコマンドの働き。
enum class PortAction : std::uint8_t { Go, Fail, Retry, Abort };

// stack[box_index] の番兵のポートの行を DebugConsole::write に書き、止まるポートなら 1 行読む。ball は例外のポートで
// 投げられた項、ほかのポートでは nullptr。口が無ければ何も書かずに Go。
[[nodiscard]] PortAction debug_port(Impl& machine, DebugPort port, std::size_t box_index, const Cell* ball);

}  // namespace mosa
