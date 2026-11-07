#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/exe_path.hpp"
#include "mosa/heap.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/repl.hpp"
#include "mosa/terminal.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
  // 何かを読み書きする前に、Windows のコンソールを UTF-8 にし、標準の流れを binary にする。他の OS では何もしない。
  mosa::prepare_console();
  // ファイル名も -g のゴールも原子になるので、UTF-8 で受ける。
  const std::vector<std::string> owned = mosa::command_line_utf8(argc, argv);
  const std::vector<std::string_view> args(owned.begin(), owned.end());
  // 端末でない標準入力は先に読まない。それは Prolog の user_input で、read/1 などが読む。
  // 先に読み切ると read/1 が end_of_file になり、閉じない入力では --version も返らない。
  bool tty = mosa::standard_input_is_terminal();
  // MOSA_TRANSCRIPT=1 はマニュアルの検査の補助（MOSA_GC_STRESS と同じく試験用）。管の入力を端末として扱い、読んだ行を
  // 書き返すので、stdout と stderr を一つにした出力が端末の表示と同じになる。本当の端末では端末が映すので書き返さない。
  const auto transcript = mosa::environment_path("MOSA_TRANSCRIPT");
  if (!tty && transcript && mosa::utf8_from_path(*transcript) == "1") {
    tty = true;
    mosa::set_standard_input_echo(true);
  }
  // 上限は --heap-max → MOSA_HEAP_MAX → 既定。API は環境変数を読まないので、ここで読んで渡す。
  const std::size_t env_limit = mosa::heap_max_from_environment();
  // 答えの行と警告は端末でなくてもその場で書く。終わりにまとめて書くと、-g を複数書いたとき前のゴールの答えが
  // 後のゴールの write の出力より後ろに来る（レビュー L10 の敵対的レビュー A1）。
  try {
    const mosa::SessionResult result =
        mosa::run_session(args, std::string_view{}, tty, env_limit, &std::cout, &std::cerr);
    return result.code;
  } catch (...) {
    if (!mosa::is_heap_exhaustion()) throw;
    // run_session の中で止まった分は run_session が知らせる。ここへ来るのは外側で溢れた場合だけ。
    std::cerr << mosa::render_message_fresh(mosa::default_message_language(), mosa::heap_stop_key(),
                                            std::array{mosa::heap_limit_mib_text(env_limit)})
              << "\n";
    return 2;
  }
}
