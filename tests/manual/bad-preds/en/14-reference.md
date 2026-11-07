# Reference

A fixture for `mosatest --manual` (`manual_fixture_bad_preds` in `tests/tests.cmake`). The example passes, but every
table below differs from Mosa.

<!-- check: query -->
```text
?- X = 1.
X = 1.
```

## library(between)

`succ/2` is left out and `nope/1` is made up.

<!-- predicates: between -->
| Predicate | Meaning |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X is an integer from Low to High. |
| `nope/1` | Not a predicate of the library. |
| `plus/3` | `plus(X, Y, Z)`: Z is X + Y. |

## Built-in predicates

`$call_cleanup/2` is an internal helper and `nope/0` is made up; the other built-in predicates are left out.

<!-- predicates: system -->
| Predicate | Meaning |
| --- | --- |
| `atom/1` | `atom(X)`: X is an atom. |
| `$call_cleanup/2` | An internal helper. |
| `nope/0` | Not a built-in predicate. |

## Message catalog

A message catalog is not a library whose predicates a table lists.

<!-- predicates: messages_en -->
| Predicate | Meaning |
| --- | --- |
| `message/2` | A message of the catalog. |
