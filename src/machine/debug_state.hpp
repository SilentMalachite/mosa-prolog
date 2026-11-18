#pragma once

// デバッガの状態（ADR 0028）。デバッグの入り切りは旗 debug と同じ Impl::debug_on を使い、ここには持たない。
// console は mosa のセッションだけが入れる。無ければ箱を作らない（タスク 5）。

#include "mosa/messages.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

enum class DebugMode : std::uint8_t { Creep, Skip, Leap };
enum class DebugPort : std::uint8_t { Call, Exit, Redo, Fail, Exception };

// leashed の 1 ビットが 1 つのポート。ビットは 1 << static_cast<int>(port)。
inline constexpr std::uint8_t kAllPorts = 0x1F;

struct SpyKey {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

struct DebugConsole {
  std::function<void(std::string_view)> write;  // ポートの行と、コマンドが書く行
  std::function<bool(std::string&)> read_line;  // 1 行読む。偽なら入力の終わり。空の関数なら読まない
};

struct DebugState {
  DebugMode mode = DebugMode::Leap;
  std::uint8_t leashed = kAllPorts;
  std::vector<SpyKey> spies;
  std::optional<DebugConsole> console;
  std::uint64_t next_box = 1;
  std::uint64_t skip_box = 0;            // 0 は skip の対象なし
  std::array<std::string, 5> port_names;  // 文面の表から 1 度だけ引く
  std::optional<MessageLanguage> names_lang;
};

}  // namespace mosa
