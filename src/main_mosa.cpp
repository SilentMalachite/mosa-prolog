#include "mosa/console.hpp"
#include "mosa/heap.hpp"
#include "mosa/messages.hpp"
#include "mosa/repl.hpp"
#include "mosa/terminal.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
  // 何かを読み書きする前に、Windows のコンソールを UTF-8 にする。他の OS では何もしない。
  mosa::prepare_console();
  std::vector<std::string> owned;
  std::vector<std::string_view> args;
  owned.reserve(static_cast<std::size_t>(argc));
  args.reserve(static_cast<std::size_t>(argc));
  for (int index = 0; index < argc; ++index) {
    owned.emplace_back(argv[index]);
    args.emplace_back(owned.back());
  }
  // 端末でない標準入力は先に読まない。それは Prolog の user_input で、read/1 などが読む。
  // 先に読み切ると read/1 が end_of_file になり、閉じない入力では --version も返らない。
  const bool tty = mosa::standard_input_is_terminal();
  try {
    const mosa::SessionResult result = mosa::run_session(args, std::string_view{}, tty, std::size_t{1} << 20,
                                                         tty ? &std::cout : nullptr, tty ? &std::cerr : nullptr);
    if (!tty) {
      std::cout << result.out;
      std::cerr << result.err;
    }
    return result.code;
  } catch (const mosa::ResourceLimit&) {
    std::cerr << mosa::render_message_fresh(mosa::default_message_language(), "heap_limit") << "\n";
    return 2;
  }
}
