章ごとの Prolog 自前テストを置く。各ファイルは `test(名前, ゴール, 期待)` の事実を並べ、`mosatest tests/pl/` が読み込んで実行する。期待は `true`、`fail`、`error(Formal)`、`solutions([X = 1, X = 2])` の 4 種で、エラーは Formal 項だけを比べる。

- `runner.pl` は `mosatest` 自体の動作確認である
- `japanese.pl` は日本語とスカラー値の数え方のテストで、フェーズ6の差分テストにも含める
- `nrev.pl` と `queens.pl` はフェーズ2の完了条件のプログラムである
- `syntax.pl` は読み書き往復の例であり、ゴールとしては実行しない。`mosatest --roundtrip` が使う
- `*_dir.pl` はディレクティブを含むファイルの読み込みを確かめる
