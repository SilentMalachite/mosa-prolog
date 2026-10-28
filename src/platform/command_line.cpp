// コマンドラインの引数を UTF-8 で受ける（SPEC 第4節、計画 Task 15b）。
// テスト: tests/cpp/console_test.cpp、tests/cpp/mosa_japanese_arg_test.cmake
#include "mosa/command_line.hpp"

#include "mosa/console.hpp"

#ifdef _WIN32
#include <memory>
#include <string_view>
#include <windows.h>
#include <shellapi.h>
#endif

namespace mosa {
namespace {

std::vector<std::string> narrow_args(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int index = 0; index < argc; ++index) args.emplace_back(argv[index]);
  return args;
}

#ifdef _WIN32

struct LocalDeleter {
  void operator()(LPWSTR* block) const { LocalFree(block); }
};

#endif

}  // namespace

std::vector<std::string> command_line_utf8(int argc, char** argv) {
#ifdef _WIN32
  int count = 0;
  const std::unique_ptr<LPWSTR, LocalDeleter> wide{CommandLineToArgvW(GetCommandLineW(), &count)};
  // CommandLineToArgvW と CRT は引用符の読み方が端で違う。数が合わなければ CRT の分け方を信じる。
  if (!wide || count != argc) return narrow_args(argc, argv);
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    const std::wstring_view arg{wide.get()[index]};
    std::u16string units;
    units.reserve(arg.size());
    for (const wchar_t unit : arg) units.push_back(static_cast<char16_t>(unit));
    args.push_back(utf8_from_utf16(units));
  }
  return args;
#else
  return narrow_args(argc, argv);
#endif
}

}
