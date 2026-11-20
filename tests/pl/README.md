Prolog tests, one file per chapter. Each file lists `test(Name, Goal, Expected)` facts, which `mosatest tests/pl/` loads and runs. `Expected` is one of `true`, `fail`, `error(Formal)`, `ball(Ball)`, or `solutions([X = 1, X = 2])`, which lists every solution in order (an empty list means the goal fails); errors compare only the `Formal` term, which must subsume the `F` of a thrown `error(F, _)`, and `ball(Ball)` subsumes a whole thrown term that is not `error/2`.

- `runner.pl` checks `mosatest` itself
- `japanese.pl` tests Japanese text and scalar-value counting; it is also part of the phase 6 differential tests
- `nrev.pl` and `queens.pl` are the programs in the phase 2 exit criteria
- `bench_<name>.pl` include the programs in `bench/` and run their first-solution checks and a short `bench(N)`, so the differential test compares them too
- `syntax.pl` holds round-trip examples and is not run as goals; `mosatest --roundtrip` uses it
- `*_dir.pl` check loading files that contain directives
- `fixtures/` holds directive-only files that `control.pl` and `operators.pl` consult; they contain no tests
- `modules/` holds the files that `modules.pl`, `japanese.pl`, `control.pl` and `include_goal_dir.pl` load; `mosatest tests/pl` does not run them as tests

章ごとの Prolog 自前テストを置く。各ファイルは `test(名前, ゴール, 期待)` の事実を並べ、`mosatest tests/pl/` が読み込んで実行する。期待は `true`、`fail`、`error(Formal)`、`ball(Ball)`、`solutions([X = 1, X = 2])` の 5 種で、エラーは Formal 項だけを比べ、Formal が投げた `error(F, _)` の `F` を包摂しなければならない。`ball(Ball)` は、`error/2` でない投げた項の全体を包摂で比べる。`solutions` は解をすべて順に並べ、空のリストはゴールが失敗することを表す。

- `runner.pl` は `mosatest` 自体の動作確認である
- `japanese.pl` は日本語とスカラー値の数え方のテストで、フェーズ6の差分テストにも含める
- `nrev.pl` と `queens.pl` はフェーズ2の完了条件のプログラムである
- `bench_<名前>.pl` は `bench/` のプログラムを include し、最初の解の確かめと短い `bench(N)` を流す。差分テストもこれを比べる
- `syntax.pl` は読み書き往復の例であり、ゴールとしては実行しない。`mosatest --roundtrip` が使う
- `*_dir.pl` はディレクティブを含むファイルの読み込みを確かめる
- `fixtures/` は `control.pl` と `operators.pl` が consult する、ディレクティブだけの部品を置く。テストは含まない
- `modules/` は `modules.pl`、`japanese.pl`、`control.pl`、`include_goal_dir.pl` が読むファイルを置く。`mosatest tests/pl` はこれをテストとしては実行しない
