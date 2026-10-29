#include "mosa/bench.hpp"
#include "mosa/command_line.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/roundtrip.hpp"
#include "mosa/suite.hpp"
#include "mosa/version.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

int usage() {
  std::cerr << mosa::render_message_fresh(mosa::default_message_language(), "mosatest_usage") << '\n';
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  // パスは UTF-8 で受け、path_from_utf8 で作る。リポジトリの場所に日本語があっても、どの OS でも同じに読む。
  const std::vector<std::string> owned = mosa::command_line_utf8(argc, argv);
  const std::vector<std::string_view> args(owned.begin(), owned.end());
  for (std::size_t index = 1; index < args.size(); ++index) {
    if (args[index] == "--version") {
      std::cout << mosa::version_line() << '\n';
      return 0;
    }
  }
  if (args.size() == 3 && args[1] == "--roundtrip") {
    std::ifstream input(mosa::path_from_utf8(args[2]), std::ios::binary);
    if (!input) return usage();
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return mosa::run_roundtrip(buffer.str());
  }
  if (args.size() >= 3 && args[1] == "--suite") {
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
  if (args.size() == 2) return mosa::run_suite(args[1]);
  return usage();
}
