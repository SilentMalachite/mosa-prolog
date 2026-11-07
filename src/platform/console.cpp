// 標準入力とコンソール（SPEC 第4節、計画 Task 14）。Windows のコンソールだけ UTF-16 で読み、Windows の標準の流れは
// binary にする（レビュー L8）。テスト: tests/cpp/console_test.cpp、tests/cpp/mosa_utf8_pipe_test.cmake、
// tests/mosac/std_binary_test.cmake、tests/cpp/answer_prompt_test.cpp、tests/cpp/transcript_test.cmake
#include "mosa/console.hpp"

#include <cstdio>
#include <iostream>
#include <utility>

#ifdef _WIN32
#include <cstdlib>
#include <fcntl.h>
#include <initializer_list>
#include <io.h>
#include <windows.h>
#endif

namespace mosa {

// 定義は src/builtins/atoms.cpp。符号化を二つ持たない。
void append_utf8_code(std::string& text, char32_t code);

namespace {

enum class ConsoleKind { NotConsole, Line, End };

struct ConsoleRead {
  ConsoleKind kind = ConsoleKind::NotConsole;
  std::string text;
};

bool high_surrogate(char16_t unit) { return unit >= 0xD800 && unit <= 0xDBFF; }
bool low_surrogate(char16_t unit) { return unit >= 0xDC00 && unit <= 0xDFFF; }

#ifdef _WIN32

UINT saved_input_cp = 0;
UINT saved_output_cp = 0;

// コードページはプロセスではなくコンソールのものなので、終わるときに戻す。戻さないと親のシェルの表示が変わる。
void restore_console() {
  if (saved_input_cp != 0) SetConsoleCP(saved_input_cp);
  if (saved_output_cp != 0) SetConsoleOutputCP(saved_output_cp);
}

ConsoleRead read_console() {
  const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  if (handle == INVALID_HANDLE_VALUE || handle == nullptr || GetConsoleMode(handle, &mode) == 0) return {};
  std::u16string line;
  std::wstring buffer(512, L'\0');
  while (line.empty() || line.back() != u'\n') {
    DWORD got = 0;
    if (ReadConsoleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr) == 0 || got == 0) break;
    for (DWORD index = 0; index < got; ++index) line.push_back(static_cast<char16_t>(buffer[index]));
  }
  if (line.empty()) return ConsoleRead{ConsoleKind::End, {}};
  auto text = console_line_text(line);
  if (!text) return ConsoleRead{ConsoleKind::End, {}};
  return ConsoleRead{ConsoleKind::Line, std::move(*text)};
}

#else

ConsoleRead read_console() { return {}; }

#endif

// 読みかけのコンソールの行（書き返す管の行も）。read/1 も get_char/1 も REPL も同じ行を分けて読むので、1 か所に持つ。
std::string pending;
std::size_t pending_at = 0;
bool console_ended = false;
// 先読みして返した字（後に返したものが先）。read/1 が終止符を確かめるために読んだ次の字を、読まなかったことにする。
std::string returned;
// MOSA_TRANSCRIPT=1。管から読んだ行を、端末が映すように書き返す。
bool echo_input = false;

// 端末は行を Enter でまとめて渡し、その行は打ったときもう映っている。だから 1 行を読み切ってから書き返す。
// 使った字だけ書き返すと、read/1 が foo. bar. の途中で止まったとき、答えが行の残りより前に出る。
// 管の終わりは Windows のコンソールの Ctrl+Z と同じ End にする。standard_input_clear が下ろすのも同じ。
ConsoleRead read_echoed_line() {
  std::string line;
  if (!std::getline(std::cin, line)) return ConsoleRead{ConsoleKind::End, {}};
  if (!std::cin.eof()) line.push_back('\n');
  std::cout << line;
  std::cout.flush();
  return ConsoleRead{ConsoleKind::Line, std::move(line)};
}

}  // namespace

void prepare_console() {
#ifdef _WIN32
  // 改行はどの OS でも LF の 1 字で、C 実行時ライブラリのテキストモードに任せない（iso-notes「7.10.2 と 8.12」の
  // 決定 A）。テキストモードはパイプの 0x1A で入力を終わらせ、CRLF の CR を落とし、nl を CRLF で書く。読み書きの
  // 前に変える。標準の流れの無いプロセスでは _fileno が負を返し、それを _setmode に渡すと実行時ライブラリが落ちる。
  for (std::FILE* stream : {stdin, stdout, stderr}) {
    const int descriptor = _fileno(stream);
    if (descriptor >= 0) _setmode(descriptor, _O_BINARY);
  }
  if (saved_input_cp != 0) return;
  saved_input_cp = GetConsoleCP();
  saved_output_cp = GetConsoleOutputCP();
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
  std::atexit(restore_console);
#endif
}

std::optional<std::string> read_console_line() {
  ConsoleRead read = read_console();
  if (read.kind != ConsoleKind::Line) return std::nullopt;
  return std::move(read.text);
}

std::string utf8_from_utf16(std::u16string_view raw) {
  std::string text;
  for (std::size_t index = 0; index < raw.size(); ++index) {
    const char16_t unit = raw[index];
    if (high_surrogate(unit) && index + 1 < raw.size() && low_surrogate(raw[index + 1])) {
      const char32_t code = 0x10000 + ((static_cast<char32_t>(unit) - 0xD800) << 10) +
                            (static_cast<char32_t>(raw[index + 1]) - 0xDC00);
      append_utf8_code(text, code);
      ++index;
      continue;
    }
    if (high_surrogate(unit) || low_surrogate(unit)) {
      append_utf8_code(text, U'\uFFFD');
      continue;
    }
    append_utf8_code(text, static_cast<char32_t>(unit));
  }
  return text;
}

std::optional<std::string> console_line_text(std::u16string_view raw) {
  if (!raw.empty() && raw.front() == char16_t{0x1A}) return std::nullopt;
  // CRLF の CR だけ落とす。サロゲートは CR を挟まないので、落としてから変換しても対は崩れない。
  std::u16string line;
  line.reserve(raw.size());
  for (std::size_t index = 0; index < raw.size(); ++index) {
    if (raw[index] == u'\r' && index + 1 < raw.size() && raw[index + 1] == u'\n') continue;
    line.push_back(raw[index]);
  }
  return utf8_from_utf16(line);
}

int standard_input_get() {
  if (!returned.empty()) {
    const auto byte = static_cast<unsigned char>(returned.back());
    returned.pop_back();
    return byte;
  }
  while (pending_at >= pending.size()) {
    if (console_ended) return EOF;
    ConsoleRead read = read_console();
    if (read.kind == ConsoleKind::NotConsole) {
      if (!echo_input) return std::cin.get();
      read = read_echoed_line();
    }
    if (read.kind == ConsoleKind::End) {
      console_ended = true;
      return EOF;
    }
    pending = std::move(read.text);
    pending_at = 0;
  }
  return static_cast<unsigned char>(pending[pending_at++]);
}

void set_standard_input_echo(bool on) { echo_input = on; }

void standard_input_unget(char byte) { returned.push_back(byte); }

void standard_input_drop_returned_line() {
  while (!returned.empty()) {
    const char byte = returned.back();
    returned.pop_back();
    if (byte == '\n') return;
  }
}

void standard_input_clear() {
  console_ended = false;
  std::cin.clear();
  // std::cin は stdin の FILE を通して読む。macOS などの stdio は FILE の終わりの印も残し、
  // それを下ろさないと端末に字が届いても getc が EOF を返し続ける。
  std::clearerr(stdin);
}

}
