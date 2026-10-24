#include "mosa/version.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  for (int index = 1; index < argc; ++index) {
    if (std::string_view{argv[index]} == "--version") {
      std::cout << mosa::version_line() << '\n';
      return 0;
    }
  }
  std::cerr << "mosa: このコマンドはフェーズ0では使えません\n";
  return 1;
}
