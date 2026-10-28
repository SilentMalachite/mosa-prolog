#include "harness.hpp"
#include "mosa/console.hpp"

#include <optional>
#include <string>

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

// ctest の標準入力はコンソールではない。どの OS でも呼び手は今の読み取りに戻る。
void check_not_console() {
  mosa::prepare_console();
  mosa_test::check(!mosa::read_console_line().has_value(), "stdin under ctest is not a console");
}

int main() {
  check_console_line_text();
  check_not_console();
  return mosa_test::failures();
}
