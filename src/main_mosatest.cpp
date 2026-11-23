#include "mosa/bench.hpp"
#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/diff.hpp"
#include "mosa/manual.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/roundtrip.hpp"
#include "mosa/suite.hpp"
#include "mosa/version.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

int usage(int code = 1) {
  std::cerr << mosa::render_message_fresh(mosa::default_message_language(), "mosatest_usage") << '\n';
  return code;
}

// ディレクトリも ifstream で開けてしまい、空の原文として黙って 0 で通った（レビュー L-42）。開けないファイルと同じく
// 「開けません」で 1 にする。
int roundtrip_file(std::string_view name) {
  const std::filesystem::path path = mosa::path_from_utf8(name);
  std::error_code error;
  std::ifstream input(path, std::ios::binary);
  if (!input || std::filesystem::is_directory(path, error)) {
    std::cerr << mosa::render_message_fresh(mosa::default_message_language(), "cannot_open_file",
                                            std::array{std::string{name}})
              << '\n';
    return 1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return mosa::run_roundtrip(buffer.str());
}

}  // namespace

int main(int argc, char** argv) {
  // 何かを読み書きする前に、Windows のコンソールを UTF-8 にし、標準の流れを binary にする。mosatest <file> も
  // Prolog の user_input と user_output を動かすので、mosa と同じにする（L8 の敵対的レビュー A3）。
  mosa::prepare_console();
  // パスは UTF-8 で受け、path_from_utf8 で作る。リポジトリの場所に日本語があっても、どの OS でも同じに読む。
  const std::vector<std::string> owned = mosa::command_line_utf8(argc, argv);
  const std::vector<std::string_view> args(owned.begin(), owned.end());
  // --version と --help は先頭の位置だけで見る（mosac と同じ）。後ろの --version は副命令の引数であり、前はどこに
  // あっても版を出して 0 で終えた（レビュー L-42）。--help は頼まれた出力なので標準出力へ出して 0。
  if (args.size() >= 2 && args[1] == "--version") {
    std::cout << mosa::version_line() << '\n';
    return 0;
  }
  if (args.size() >= 2 && args[1] == "--help") {
    std::cout << mosa::render_message_fresh(mosa::default_message_language(), "mosatest_usage") << '\n';
    return 0;
  }
  if (args.size() >= 2 && args[1] == "--roundtrip") return args.size() == 3 ? roundtrip_file(args[2]) : usage();
  if (args.size() >= 2 && args[1] == "--suite") {
    const std::vector<std::string_view> rest(args.begin() + 2, args.end());
    const mosa::SuiteArgs parsed = mosa::parse_suite_args(rest);
    if (!parsed.ok) return usage();
    return mosa::run_iso_suite(parsed.dir, parsed.report);
  }
  if (args.size() >= 2 && args[1] == "--bench") {
    const std::vector<std::string_view> rest(args.begin() + 2, args.end());
    const mosa::BenchArgs parsed = mosa::parse_bench_args(rest);
    if (!parsed.ok) return usage();
    return mosa::run_bench(parsed);
  }
  if (args.size() >= 2 && args[1] == "--diff") {
    const std::vector<std::string_view> rest(args.begin() + 2, args.end());
    const mosa::DiffArgs parsed = mosa::parse_diff_args(rest);
    if (!parsed.ok) return usage();
    return mosa::run_diff(parsed);
  }
  // --manual の使い方の誤りは 2。1 は例の失敗に取っておく（リリース 0.1.0 Task 4）。
  if (args.size() >= 2 && args[1] == "--manual") {
    const std::vector<std::string_view> rest(args.begin() + 2, args.end());
    const mosa::ManualArgs parsed = mosa::parse_manual_args(rest);
    if (!parsed.ok) return usage(2);
    return mosa::run_manual(parsed);
  }
  // 試験のファイルとディレクトリの名前は - で始まらない。前は一つきりの --suite や知らない旗をファイルの名前と取った。
  if (args.size() == 2 && !args[1].empty() && args[1].front() != '-') return mosa::run_suite(args[1]);
  return usage();
}
