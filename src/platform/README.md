OS-dependent code. OS branches go nowhere else. `terminal.cpp` checks whether standard input is a terminal. `executable.cpp` finds the running executable so `library(Name)` can open `lib/` beside it.

OS 依存コードを置く。ここ以外に OS 分岐を書かない。`terminal.cpp` は標準入力が端末かを見る。`executable.cpp` は実行ファイルの位置を取り、`library(Name)` が隣の `lib/` を開けるようにする。
