# Reference

A fixture for the predicate tables of `mosatest --manual` (`manual_fixture_good` in `tests/tests.cmake`). The table
lists the exports of `library(between)` exactly, so it passes.

## library(between)

<!-- predicates: between -->
| Predicate | Meaning |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X is an integer from Low to High. |
| `succ/2` | `succ(X, Y)`: Y is X + 1, both natural numbers. |
| `plus/3` | `plus(X, Y, Z)`: Z is X + Y. |

## Directives

The section above ends at this heading, so this table is not compared with any list of predicates.

| Directive | Meaning |
| --- | --- |
| `use_module/1` | Load a module and import everything it exports. |
