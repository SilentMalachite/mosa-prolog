#pragma once

// デバッガのポート（ADR 0028）。ポートを見せるか、止まってコマンドを読むかを 1 か所で決める。箱の出入り
// （debug_box.cpp）が呼ぶ。

#include "internal.hpp"

#include <cstddef>
#include <cstdint>

namespace mosa {

// stack[box_index] の番兵のポートの行を DebugConsole::write に書き、止まるポートなら 1 行読んでコマンドを当てる。ball は
// 例外のポートで投げられた項、ほかのポートでは nullptr。口が無いか、デバッグが切りか、進み方で見せないポートなら、
// 何も書かずに Go。Abort なら abort_now を立て、ip を空にしてから返す。PortAction は debug_state.hpp。
// g w + - h ? と知らない入力は書いてから同じポートをもう一度問い、入力の終わりは n と同じにする。
[[nodiscard]] PortAction debug_port(Impl& machine, DebugPort port, std::size_t box_index, const Cell* ball);

// stack[box_index] の箱のポートを debug_port が書くか。状態は変えない（skip の箱に着いて creep に戻すのは
// debug_port）。throw_box が投げられた項を取り込む前に聞く。
[[nodiscard]] bool debug_port_shown(Impl& machine, std::size_t box_index);

}  // namespace mosa
