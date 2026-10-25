#pragma once

#include <cstddef>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

namespace mosa {

struct SessionResult {
  int code = 0;
  std::string out;
  std::string err;
};

[[nodiscard]] SessionResult run_session(std::span<const std::string_view> args, std::string_view input, bool tty,
                                       std::size_t heap_limit = std::size_t{1} << 20,
                                       std::ostream* live_out = nullptr, std::ostream* live_err = nullptr);

}
