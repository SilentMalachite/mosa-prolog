#include "harness.hpp"
#include "mosa/command_line.hpp"
#include "mosa/console.hpp"

#include <optional>
#include <string>
#include <vector>

// SPEC 第4節。Windows のコンソールは UTF-16 で読む。その行を UTF-8 にする部分は OS に依らないので、どの OS でも試す。
void check_console_line_text() {
  const std::u16string oya = u"親(太郎).\r\n";
  mosa_test::check(mosa::console_line_text(oya) == std::optional<std::string>{"親(太郎).\n"}, "crlf to lf");
  const std::u16string yoshi = u"𠮷\n";
  mosa_test::check(mosa::console_line_text(yoshi) == std::optional<std::string>{"\xF0\xA0\xAE\xB7\n"}, "surrogate pair");
  const std::u16string lone{char16_t{0xD842}, u'a'};
  mosa_test::check(mosa::console_line_text(lone) == std::optional<std::string>{"\xEF\xBF\xBD" "a"}, "lone surrogate");
  const std::u16string ctrl_z{char16_t{0x1A}, u'\r', u'\n'};
  mosa_test::check(!mosa::console_line_text(ctrl_z).has_value(), "ctrl-z is end of input");
  const std::u16string bare_cr{u'a', u'\r', u'b'};
  mosa_test::check(mosa::console_line_text(bare_cr) == std::optional<std::string>{"a\rb"}, "lone cr kept");
}

// Windows の引数（CommandLineToArgvW）も UTF-16。行の扱いを持たない素の変換（計画 Task 15b）。
void check_utf8_from_utf16() {
  mosa_test::check(mosa::utf8_from_utf16(u"日本語の引数.pl") == "日本語の引数.pl", "bmp");
  mosa_test::check(mosa::utf8_from_utf16(u"𠮷") == "\xF0\xA0\xAE\xB7", "surrogate pair");
  const std::u16string lone{u'a', char16_t{0xDC00}};
  mosa_test::check(mosa::utf8_from_utf16(lone) == "a\xEF\xBF\xBD", "lone low surrogate");
  mosa_test::check(mosa::utf8_from_utf16(u"a\r\n") == "a\r\n", "crlf kept");
  const std::u16string ctrl_z{char16_t{0x1A}};
  mosa_test::check(mosa::utf8_from_utf16(ctrl_z) == "\x1A", "ctrl-z is a character");
}

// ctest の標準入力はコンソールではない。どの OS でも呼び手は今の読み取りに戻る。
void check_not_console() {
  mosa::prepare_console();
  mosa_test::check(!mosa::read_console_line().has_value(), "stdin under ctest is not a console");
}

// ctest が渡す引数は ASCII だけ。どの OS でも argv と同じ並びが返る。日本語の引数は mosa_japanese_arg で見る。
void check_command_line(int argc, char** argv) {
  const std::vector<std::string> args = mosa::command_line_utf8(argc, argv);
  bool same = args.size() == static_cast<std::size_t>(argc);
  for (std::size_t index = 0; same && index < args.size(); ++index) same = args[index] == argv[index];
  mosa_test::check(same, "command line matches argv");
}

int main(int argc, char** argv) {
  check_command_line(argc, argv);
  check_console_line_text();
  check_utf8_from_utf16();
  check_not_console();
  return mosa_test::failures();
}
