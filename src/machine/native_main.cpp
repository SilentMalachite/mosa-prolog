// 計画 Task 5。mosac が出す main.cpp の起動と終わり。終了コードの規則は mosa と同じ run_main_goal にある。
// 生成コードは runtime.hpp しか見ないので、コンソールと引数の扱いもここで包む。

#include "internal.hpp"

#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/heap.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/repl.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::rt {

std::unique_ptr<Machine> create_machine() {
  const std::size_t limit = heap_max_from_environment();
  try {
    return Machine::create(limit);
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    std::cerr << render_message_fresh(default_message_language(), "heap_limit_size",
                                      std::array{heap_limit_mib_text(limit)})
              << '\n';
    std::exit(2);
  }
}

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

namespace {

// 読み込みの結果の見方。終わるなら終了コードを返し、続けるなら nullopt。finish と finish_interactive で同じにする。
std::optional<int> load_outcome(Impl& impl, bool loaded) {
  // 指示の halt/0,1 は読み込みをその場で止め、その値で終わる（mosa と同じ。SPEC 第6節の実行ファイルの振る舞い）。
  if (impl.halt_now) return impl.halt_code;
  if (!loaded) {
    std::cerr << impl.diagnostic << '\n';
    // ヒープの上限は mosa の consult と同じ 2、ほかの読み込みの失敗は 1（計画 Task 10 の持ち越し）。
    return impl.load_heap_limit ? 2 : 1;
  }
  return std::nullopt;
}

// 上限に達したマシンのヒープではカタログを読めないので、新しいマシンで文面を作る。終了コードは 2。
int report_heap_limit(Machine& machine) {
  const std::string limit = heap_limit_mib_text(machine.heap().limit());
  std::cerr << render_message_fresh(default_message_language(), "heap_limit_size", std::array{limit}) << '\n';
  return 2;
}

}  // namespace

int finish_interactive(Machine& machine, bool loaded) {
  if (const auto code = load_outcome(engine_impl(machine), loaded)) return *code;
  try {
    return run_prepared_session(machine, &std::cout, &std::cerr).code;
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    return report_heap_limit(machine);
  }
}

int finish(Machine& machine, bool loaded) {
  Impl& impl = engine_impl(machine);
  if (const auto code = load_outcome(impl, loaded)) return *code;
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
