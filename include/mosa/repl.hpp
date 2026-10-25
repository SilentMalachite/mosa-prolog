#pragma once

#include <span>
#include <string>
#include <string_view>

namespace mosa {

struct SessionResult {
  int code = 0;
  std::string out;
  std::string err;
};

[[nodiscard]] SessionResult run_session(std::span<const std::string_view> args, std::string_view input, bool tty);

}
