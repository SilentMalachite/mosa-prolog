# 参照

`mosatest --manual` の述語の表の見本（`tests/tests.cmake` の `manual_fixture_good`）。表は `library(between)` の
公開する述語をちょうど並べているので通る。

## library(between)

<!-- predicates: between -->
| 述語 | 意味 |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X は Low から High までの整数。 |
| `succ/2` | `succ(X, Y)`: Y は X + 1。どちらも自然数。 |
| `plus/3` | `plus(X, Y, Z)`: Z は X + Y。 |

## 指令

上の区切りはこの見出しで終わるので、この表はどの述語の一覧とも比べない。

| 指令 | 意味 |
| --- | --- |
| `use_module/1` | モジュールを読み、公開するものをすべて取り込む。 |
