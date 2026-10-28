OS-dependent code. OS branches go nowhere else. `terminal.cpp` tells whether standard input is a terminal, `exe_path.cpp` finds the running executable and reads environment variables, and `console.cpp` sets the Windows console to UTF-8 and is the one byte source for standard input (a Windows console is read as UTF-16 with `ReadConsoleW`).

OS 依存コードを置く。ここ以外に OS 分岐を書かない。`terminal.cpp` は標準入力が端末かを判定し、`exe_path.cpp` は動いている実行ファイルの場所と環境変数を読み、`console.cpp` は Windows のコンソールを UTF-8 にして、標準入力のただ一つの読み取り口になる（Windows のコンソールは `ReadConsoleW` で UTF-16 として読む）。
