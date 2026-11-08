#pragma once

// 標準入力とコンソール（SPEC 第4節）。定義は src/platform/console.cpp だけに置く。
#include <optional>
#include <string>
#include <string_view>

namespace mosa {

// Windows ではコンソールの入出力のコードページを UTF-8 にし、stdin、stdout、stderr を binary にする（改行は LF だけ、
// 0x1A はただのバイト。iso-notes「7.10.2 と 8.12」）。他の OS では何もしない。何かを読み書きする前に呼ぶ。
void prepare_console();
// 標準入力が Windows のコンソールのときだけ、ReadConsoleW で 1 行を読み UTF-8 にして返す。
// それ以外は nullopt で、呼び手は今の読み取りに戻る。入力の終わりも nullopt。
[[nodiscard]] std::optional<std::string> read_console_line();
// UTF-16 を UTF-8 にする。行の扱いはしない。対の無いサロゲートは U+FFFD。
[[nodiscard]] std::string utf8_from_utf16(std::u16string_view raw);
// ReadConsoleW の 1 行を UTF-8 にする。CRLF は LF、対の無いサロゲートは U+FFFD、行頭の Ctrl+Z は入力の終わり（nullopt）。
[[nodiscard]] std::optional<std::string> console_line_text(std::u16string_view raw);
// 標準入力の 1 バイト。コンソールの行を読み終えるまでそこから返し、他は std::cin から読む。終わりは EOF。
[[nodiscard]] int standard_input_get();
// 真にすると、Windows のコンソールでない標準入力は std::cin から 1 行（\n まで、終わりなら残り全部）を読み、
// std::cout に書いて flush してから、その行を 1 バイトずつ返す。端末が打った行を映すのと同じ表示を管から得るため
// （MOSA_TRANSCRIPT=1、マニュアルの検査）。standard_input_unget で返された字は書き返さない。既定は偽。
void set_standard_input_echo(bool on);
// standard_input_get で読んだ字を返す。次の standard_input_get が返した順の逆に返す（レビュー M12）。
void standard_input_unget(char byte);
// 入力の終わりの印を下ろす。eof_action(reset) と、端末で Ctrl+D の後に読み続けるため。
void standard_input_clear();
// standard_input_unget で返された字を、最初の改行まで（無ければ全部）捨てる。read/1 が終止符を確かめるために読んだ
// 字で、利用者がもう打った字なので待たずに捨てられる。答えの後の ; の促しが、read/1 の残した foo. の後の改行を
// 答えにしないため（レビュー L15）。返された字より先は読まない。端末で行の途中を Ctrl+D で送った後は、その先を
// 待つと次に打つ行を捨ててしまう（L15 の Codex レビューと敵対的レビュー A1）。
void standard_input_drop_returned_line();

}
