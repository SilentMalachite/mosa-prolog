#pragma once

#include <cstddef>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

namespace mosa {

class Machine;

struct SessionResult {
  int code = 0;
  std::string out;
  std::string err;
};

[[nodiscard]] SessionResult run_session(std::span<const std::string_view> args, std::string_view input, bool tty,
                                       std::size_t heap_limit = std::size_t{1} << 20,
                                       std::ostream* live_out = nullptr, std::ostream* live_err = nullptr);

// 読み込み済みの machine の上で REPL だけを動かす（mosac build --interactive。計画 Task 14）。標準入力が端末でなくても
// 行を読む。-g も initialization(main, main) も動かさない。run_session と同じ prompt_loop を通る。
[[nodiscard]] SessionResult run_prepared_session(Machine& machine, std::ostream* live_out, std::ostream* live_err);

}
