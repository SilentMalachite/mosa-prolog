#include "mosa/roundtrip.hpp"
#include "mosa/version.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

namespace {

int usage() {
  std::cerr << "mosatest: 使い方: mosatest --roundtrip <ファイル>\n";
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
  if (argc != 3 || std::string_view{argv[1]} != "--roundtrip") return usage();
  std::ifstream input(argv[2], std::ios::binary);
  if (!input) return usage();
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return mosa::run_roundtrip(buffer.str());
}
