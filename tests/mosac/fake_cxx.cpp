// mosac_internal_error の偽のコンパイラ（計画 Task 9）。find_toolchain は `<cxx> --version` が 0 で終わるものだけを
// コンパイラと認めるので、--version にだけは答える。ほかの呼び出しは必ず失敗し、生の誤りを標準エラーに書く。
// その文面が --verbose のときだけ mosac の出力に出ることを、テストが確かめる。
#include <cstdio>
#include <span>
#include <string_view>

int main(int argc, char** argv) {
  const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
  if (args.size() == 2 && std::string_view{args[1]} == "--version") {
    std::puts("mosac-fake-cxx 1.0");
    return 0;
  }
  std::fputs("fake-cxx: always fails\n", stderr);
  return 1;
}
