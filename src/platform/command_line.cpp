// コマンドラインの引数を UTF-8 で受ける（SPEC 第4節、計画 Task 15b）。
// テスト: tests/cpp/console_test.cpp、tests/cpp/mosa_japanese_arg_test.cmake、
// tests/cpp/windows_process_test.ps1（windows_argv_quotes）
#include "mosa/command_line.hpp"

#include "mosa/console.hpp"

#include <cstddef>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mosa {
namespace {

std::vector<std::string> narrow_args(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int index = 0; index < argc; ++index) args.emplace_back(argv[index]);
  return args;
}

bool blank_at(std::u16string_view line, std::size_t at) {
  return at < line.size() && (line[at] == u' ' || line[at] == u'\t');
}

// 引数を 1 つ読む。2N 個の \ と " は N 個の \ と引用の始めか終わり、2N+1 個の \ と " は N 個の \ と " の字、
// " の前でない \ はそのまま。引用の中の "" は " の字で、引用は続く（CRT の 2008 以降の規則）。
std::u16string next_argument(std::u16string_view line, std::size_t& at) {
  std::u16string arg;
  bool quoted = false;
  while (true) {
    std::size_t slashes = 0;
    while (at < line.size() && line[at] == u'\\') {
      ++slashes;
      ++at;
    }
    bool copy = true;
    if (at < line.size() && line[at] == u'"') {
      if (slashes % 2 == 0) {
        if (quoted && at + 1 < line.size() && line[at + 1] == u'"') {
          ++at;
        } else {
          copy = false;
          quoted = !quoted;
        }
      }
      slashes /= 2;
    }
    arg.append(slashes, u'\\');
    if (at >= line.size() || (!quoted && blank_at(line, at))) break;
    if (copy) arg.push_back(line[at]);
    ++at;
  }
  return arg;
}

}  // namespace

std::vector<std::u16string> split_command_line(std::u16string_view line) {
  std::vector<std::u16string> args;
  std::size_t at = 0;
  std::u16string name;
  bool quoted = false;
  while (at < line.size() && (quoted || !blank_at(line, at))) {
    if (line[at] == u'"') {
      quoted = !quoted;
    } else {
      name.push_back(line[at]);
    }
    ++at;
  }
  args.push_back(std::move(name));
  while (true) {
    while (blank_at(line, at)) ++at;
    if (at >= line.size()) break;
    args.push_back(next_argument(line, at));
  }
  return args;
}

std::vector<std::string> command_line_utf8(int argc, char** argv) {
#ifdef _WIN32
  // CommandLineToArgvW は引用符の中の "" を CRT と違えて読み、数が合わなければ ANSI の argv に戻るしか無かった
  // （レビュー L-38 の 5）。CRT と同じ規則で GetCommandLineW を分けるので、数は argc と合う。
  std::u16string line;
  for (const wchar_t* unit = GetCommandLineW(); unit != nullptr && *unit != L'\0'; ++unit) {
    line.push_back(static_cast<char16_t>(*unit));
  }
  const std::vector<std::u16string> wide = split_command_line(line);
  if (wide.size() != static_cast<std::size_t>(argc)) return narrow_args(argc, argv);
  std::vector<std::string> args;
  args.reserve(wide.size());
  for (const std::u16string& arg : wide) args.push_back(utf8_from_utf16(arg));
  return args;
#else
  return narrow_args(argc, argv);
#endif
}

}
