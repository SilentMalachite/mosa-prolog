# 参照

`mosatest --manual` の見本（`tests/tests.cmake` の `manual_fixture_bad_lib`）。試験は `mosa` とその隣の `lib/` を
置き場に写して `bad-lib/lib/` を足すので、下のライブラリはあるが読めない。例は通る。

<!-- check: query -->
```text
?- X = 1.
X = 1.
```

## library(broken)

`broken.pl` は構文の誤りを持つ。

<!-- predicates: broken -->
| 述語 | 意味 |
| --- | --- |
| `broken/0` | 読まれない。 |

## library(nomodule)

`nomodule.pl` は節を持つが、`module/2` の指令が無い。

<!-- predicates: nomodule -->
| 述語 | 意味 |
| --- | --- |
| `nomodule/0` | 読まれない。 |

## library(renamed)

`renamed.pl` は読めるが、宣言するのはモジュール `other`。

<!-- predicates: renamed -->
| 述語 | 意味 |
| --- | --- |
| `renamed/0` | `renamed` でなく `other` が公開する。 |
