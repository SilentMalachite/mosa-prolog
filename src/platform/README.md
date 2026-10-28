OS-dependent code. OS branches go nowhere else. `terminal.cpp` tells whether standard input is a terminal, and `exe_path.cpp` finds the running executable and reads environment variables.

OS 依存コードを置く。ここ以外に OS 分岐を書かない。`terminal.cpp` は標準入力が端末かを判定し、`exe_path.cpp` は動いている実行ファイルの場所と環境変数を読む。
