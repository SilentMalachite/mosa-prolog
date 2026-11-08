# 参照

`mosatest --manual` の見本（`tests/tests.cmake` の `manual_fixture_bad_preds`）。例は通るが、下の表はどれも Mosa と
違う。

<!-- check: query -->
```text
?- X = 親.
X = 親.
```

## library(between)

`succ/2` を抜き、`nope/1` を作って足してある。

<!-- predicates: between -->
| 述語 | 意味 |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X は Low から High までの整数。 |
| `nope/1` | このライブラリの述語ではない。 |
| `plus/3` | `plus(X, Y, Z)`: Z は X + Y。 |

## 組込述語

`$call_cleanup/2` は内側の補助で、`nope/0` は作ったもの。ほかの組込述語は抜いてある。

<!-- predicates: system -->
| 述語 | 意味 |
| --- | --- |
| `atom/1` | `atom(X)`: X は原子。 |
| `$call_cleanup/2` | 内側の補助。 |
| `nope/0` | 組込述語ではない。 |

## 文面の表

文面の表は、述語を表に並べるライブラリではない。

<!-- predicates: messages_en -->
| 述語 | 意味 |
| --- | --- |
| `message/2` | 表の文面。 |
