#pragma once

// 標準入力とコンソール（SPEC 第4節）。定義は src/platform/console.cpp だけに置く。
#include <optional>
#include <string>
#include <string_view>

namespace mosa {

// Windows ではコンソールの入出力のコードページを UTF-8 にする。他の OS では何もしない。
void prepare_console();
// 標準入力が Windows のコンソールのときだけ、ReadConsoleW で 1 行を読み UTF-8 にして返す。
// それ以外は nullopt で、呼び手は今の読み取りに戻る。入力の終わりも nullopt。
[[nodiscard]] std::optional<std::string> read_console_line();
// ReadConsoleW の 1 行を UTF-8 にする。CRLF は LF、対の無いサロゲートは U+FFFD、行頭の Ctrl+Z は入力の終わり（nullopt）。
[[nodiscard]] std::optional<std::string> console_line_text(std::u16string_view raw);
// 標準入力の 1 バイト。コンソールの行を読み終えるまでそこから返し、他は std::cin から読む。終わりは EOF。
[[nodiscard]] int standard_input_get();
// 入力の終わりの印を下ろす。eof_action(reset) と、端末で Ctrl+D の後に読み続けるため。
void standard_input_clear();

}
