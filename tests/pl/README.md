Prolog tests, one file per chapter. Each file lists `test(Name, Goal, Expected)` facts, which `mosatest tests/pl/` loads and runs. `Expected` is one of `true`, `fail`, `error(Formal)`, or `solutions([X = 1, X = 2])`; errors compare only the `Formal` term.

- `runner.pl` checks `mosatest` itself
- `japanese.pl` tests Japanese text and scalar-value counting; it is also part of the phase 6 differential tests
- `nrev.pl` and `queens.pl` are the programs in the phase 2 exit criteria
- `syntax.pl` holds round-trip examples and is not run as goals; `mosatest --roundtrip` uses it
- `*_dir.pl` check loading files that contain directives

章ごとの Prolog 自前テストを置く。各ファイルは `test(名前, ゴール, 期待)` の事実を並べ、`mosatest tests/pl/` が読み込んで実行する。期待は `true`、`fail`、`error(Formal)`、`solutions([X = 1, X = 2])` の 4 種で、エラーは Formal 項だけを比べる。

- `runner.pl` は `mosatest` 自体の動作確認である
- `japanese.pl` は日本語とスカラー値の数え方のテストで、フェーズ6の差分テストにも含める
- `nrev.pl` と `queens.pl` はフェーズ2の完了条件のプログラムである
- `syntax.pl` は読み書き往復の例であり、ゴールとしては実行しない。`mosatest --roundtrip` が使う
- `*_dir.pl` はディレクティブを含むファイルの読み込みを確かめる
