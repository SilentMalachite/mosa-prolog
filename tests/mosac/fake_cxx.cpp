// mosac_internal_error の偽のコンパイラ（計画 Task 9）。find_toolchain は `<cxx> --version` が 0 で終わるものだけを
// コンパイラと認めるので、--version にだけは答える。生成コード（.mosac-cache の .cpp）のコンパイルは必ず失敗し、
// 生の誤りを標準エラーに書く。その文面が --verbose のときだけ mosac の出力に出ることを、テストが確かめる。
// kRealCxx（configure_file が作る fake_cxx_config.hpp）が空でなければ（目標 mosac_fake_cxx_forward）、
// ほかの呼び出しは本物のコンパイラに渡す。
// CMake の configure が試すコンパイルを通し、CMake の道でも生成コードの段で失敗させるためである。転送は POSIX の
// sh の引用で書く。Windows では cl 以外のコンパイラは直接の道を通るので、転送する版は使わない。
// 空なら（目標 mosac_fake_cxx）どの呼び出しも失敗し、CMake の道では configure の段で失敗する。
#include "fake_cxx_config.hpp"

#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kReal = kRealCxx;

std::string sh_quoted(std::string_view arg) {
  std::string out = "'";
  for (const char unit : arg) out += unit == '\'' ? std::string{"'\\''"} : std::string{unit};
  return out + "'";
}

bool generated_source(std::string_view arg) {
  return arg.ends_with(".cpp") && arg.find(".mosac-cache") != std::string_view::npos &&
         arg.find("CMakeFiles") == std::string_view::npos;
}

}  // namespace

int main(int argc, char** argv) {
  const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
  if (args.size() == 2 && std::string_view{args[1]} == "--version") {
    std::puts("mosac-fake-cxx 1.0");
    return 0;
  }
  bool generated = false;
  std::string command = sh_quoted(kReal);
  for (std::size_t index = 1; index < args.size(); ++index) {
    generated = generated || generated_source(args[index]);
    command += " " + sh_quoted(args[index]);
  }
  if (kReal.empty() || generated) {
    std::fputs("fake-cxx: always fails\n", stderr);
    return 1;
  }
  return std::system(command.c_str()) == 0 ? 0 : 1;
}
