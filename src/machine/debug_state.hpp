#pragma once

// デバッガの状態（ADR 0028）。デバッグの入り切りは旗 debug と同じ Impl::debug_on を使い、ここには持たない。
// console は mosa のセッションだけが入れる。無ければ箱を作らない（タスク 5）。

#include "mosa/messages.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

enum class DebugMode : std::uint8_t { Creep, Skip, Leap };
enum class DebugPort : std::uint8_t { Call, Exit, Redo, Fail, Exception };

// 止まったポートで読んだコマンドの働き。debug_port が返し、箱の出入り（debug_box.cpp）と propagate_throw が受ける。
enum class PortAction : std::uint8_t { Go, Fail, Retry, Abort };

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

// 開いている箱（armed の番兵）のスタックの添字と番号（ADR 0028）。
struct OpenBox {
  std::size_t index = 0;
  std::uint64_t number = 0;
};

struct DebugState {
  DebugMode mode = DebugMode::Leap;
  std::uint8_t leashed = kAllPorts;
  std::vector<SpyKey> spies;
  std::optional<DebugConsole> console;
  std::uint64_t next_box = 1;
  std::uint64_t skip_box = 0;            // 0 は skip の対象なし
  // 開いている箱を下から順に並べる。箱を開くときは一番上の生きている行だけを見て深さを決め、スタックを辿らない。leap では
  // 箱が少ないので、辿ると選択点の数だけかかり、選択点の下でスパイポイントを呼ぶ再帰が回数の 2 乗になる（PR #4 の敵対的
  // レビュー）。抜けた箱、切った箱、捨てた問い合わせの箱の行は、次に開くときに上から捨てる。GC もスタックより上の行を
  // 捨てて容量を返す（state_count.cpp の give_back_vectors）。指令は脇に置くスタックと一緒にこの表も脇に置く
  // （load_directive.cpp の park_exec）。
  std::vector<OpenBox> open_boxes;
  std::array<std::string, 5> port_names;  // 文面の表から 1 度だけ引く
  std::optional<MessageLanguage> names_lang;
};

// skip はその箱のポートに着くまでの、1 つの問い合わせの中だけの進み方である。問い合わせを捨てるか abort すると
// その箱はもうポートを出さず、skip のままだと次の trace まで箱が 1 つも作られない（コントローラの裁定 16）。
inline void end_skip(DebugState& state) {
  if (state.mode != DebugMode::Skip) return;
  state.mode = DebugMode::Creep;
  state.skip_box = 0;
}

}  // namespace mosa
