#include "mosa/messages.hpp"
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
  for (int index = 1; index < argc; ++index) {
    if (std::string_view{argv[index]} == "--version") {
      std::cout << mosa::version_line() << '\n';
      return 0;
    }
  }
  if (argc == 3 && std::string_view{argv[1]} == "--roundtrip") {
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) return usage();
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return mosa::run_roundtrip(buffer.str());
  }
  if (argc >= 3 && std::string_view{argv[1]} == "--suite") {
    const std::vector<std::string_view> rest(argv + 2, argv + argc);
    const mosa::SuiteArgs parsed = mosa::parse_suite_args(rest);
    if (!parsed.ok) return usage();
    return mosa::run_iso_suite(parsed.dir, parsed.report);
  }
  if (argc == 2) return mosa::run_suite(argv[1]);
  return usage();
}
