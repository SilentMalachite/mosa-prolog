#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/heap.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/messages.hpp"
#include "mosa/repl.hpp"
#include "mosa/terminal.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
  // 何かを読み書きする前に、Windows のコンソールを UTF-8 にする。他の OS では何もしない。
  mosa::prepare_console();
  // ファイル名も -g のゴールも原子になるので、UTF-8 で受ける。
  const std::vector<std::string> owned = mosa::command_line_utf8(argc, argv);
  const std::vector<std::string_view> args(owned.begin(), owned.end());
  // 端末でない標準入力は先に読まない。それは Prolog の user_input で、read/1 などが読む。
  // 先に読み切ると read/1 が end_of_file になり、閉じない入力では --version も返らない。
  const bool tty = mosa::standard_input_is_terminal();
  // 上限は --heap-max → MOSA_HEAP_MAX → 既定。API は環境変数を読まないので、ここで読んで渡す。
  const std::size_t env_limit = mosa::heap_max_from_environment();
  try {
    const mosa::SessionResult result = mosa::run_session(args, std::string_view{}, tty, env_limit,
                                                         tty ? &std::cout : nullptr, tty ? &std::cerr : nullptr);
    if (!tty) {
      std::cout << result.out;
      std::cerr << result.err;
    }
    return result.code;
  } catch (...) {
    if (!mosa::is_heap_exhaustion()) throw;
    // run_session の中で止まった分は run_session が知らせる。ここへ来るのは外側で溢れた場合だけ。
    std::cerr << mosa::render_message_fresh(mosa::default_message_language(), "heap_limit_size",
                                            std::array{mosa::heap_limit_mib_text(env_limit)})
              << "\n";
    return 2;
  }
}
