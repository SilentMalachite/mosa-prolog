#include "mosa/repl.hpp"
#include "mosa/terminal.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
  std::vector<std::string> owned;
  std::vector<std::string_view> args;
  owned.reserve(static_cast<std::size_t>(argc));
  args.reserve(static_cast<std::size_t>(argc));
  for (int index = 0; index < argc; ++index) {
    owned.emplace_back(argv[index]);
    args.emplace_back(owned.back());
  }
  const bool tty = mosa::standard_input_is_terminal();
  std::string input;
  if (!tty) {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    input = buffer.str();
  }
  const mosa::SessionResult result = mosa::run_session(args, tty ? std::string_view{} : std::string_view{input}, tty);
  std::cout << result.out;
  std::cerr << result.err;
  return result.code;
}
