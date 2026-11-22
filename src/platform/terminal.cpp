// 標準入力が端末か（SPEC 第4節）。端末でない入力は先に読まず、Prolog の user_input に残す。
// テスト: tests/cpp/mosa_stdin_test.cmake、tests/cpp/windows_process_test.ps1（windows_no_stdin）
#include "mosa/terminal.hpp"

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace mosa {

bool standard_input_is_terminal() {
#ifdef _WIN32
  // _isatty は文字の装置の NUL も端末とし、標準入力の無いプロセスの負の記述子は実行時ライブラリの検査に当たる
  // （レビュー M-42）。console.cpp の read_console と同じく、コンソールの取っ手かどうかで見る。
  const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  return handle != nullptr && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != 0;
#else
  return isatty(fileno(stdin)) != 0;
#endif
}

}
