// mosac_interactive の基準。mosa と同じ run_session を、標準入力が端末でなくても REPL として動かす
// （src/main_mosa.cpp は端末でない入力を読まない）。翻訳した実行ファイルの出力はこれと同じでなければならない。

#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/repl.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
  mosa::prepare_console();
  const std::vector<std::string> owned = mosa::command_line_utf8(argc, argv);
  const std::vector<std::string_view> args(owned.begin(), owned.end());
  return mosa::run_session(args, std::string_view{}, true, std::size_t{1} << 20, &std::cout, &std::cerr).code;
}
