// 計画 Task 5。mosac が出す main.cpp の起動と終わり。終了コードの規則は mosa と同じ run_main_goal にある。
// 生成コードは runtime.hpp しか見ないので、コンソールと引数の扱いもここで包む。

#include "internal.hpp"

#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/path_text.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::rt {

void start(Machine& machine, int argc, char** argv) {
  // 何かを読み書きする前に、Windows のコンソールを UTF-8 にする（mosa の main と同じ順）。
  prepare_console();
  std::vector<std::string> args = command_line_utf8(argc, argv);
  // mosa では -- より後ろが旗 argv になる。実行ファイルではプログラム名より後ろの全部である。
  if (!args.empty()) args.erase(args.begin());
  engine_impl(machine).program_argv = std::move(args);
}

void embed(Machine& machine, std::string_view path, std::string_view source) {
  Impl& impl = engine_impl(machine);
  impl.embedded.push_back(EmbeddedSource{utf8_from_path(canonical_key(path_from_utf8(path))), std::string{source}});
}

int finish(Machine& machine, bool loaded) {
  Impl& impl = engine_impl(machine);
  // 指示の halt/0,1 は読み込みをその場で止め、その値で終わる（mosa と同じ。SPEC 第6節の実行ファイルの振る舞い）。
  if (impl.halt_now) return impl.halt_code;
  if (!loaded) {
    std::cerr << impl.diagnostic << '\n';
    return 1;
  }
  if (!impl.main_goal) return 0;
  std::string warning;
  const int code = run_main_goal(machine, *impl.main_goal, warning);
  std::cout.flush();
  std::cerr << warning;
  return code;
}

// 標準エラーは Prolog の user_error と同じ先なので、プログラムの出力と追跡の前後が保たれる。
void trace_clause(std::string_view text) { std::cerr << text << '\n'; }

}  // namespace mosa::rt
