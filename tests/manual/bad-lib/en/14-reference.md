# Reference

A fixture for `mosatest --manual` (`manual_fixture_bad_lib` in `tests/tests.cmake`). The test copies `mosa` and the
`lib/` beside it into a staging directory and adds `bad-lib/lib/`, so the libraries below are present but cannot be
loaded. The example passes.

<!-- check: query -->
```text
?- X = 1.
X = 1.
```

## library(broken)

`broken.pl` has a syntax error.

<!-- predicates: broken -->
| Predicate | Meaning |
| --- | --- |
| `broken/0` | Never loaded. |

## library(nomodule)

`nomodule.pl` has a clause but no `module/2` directive.

<!-- predicates: nomodule -->
| Predicate | Meaning |
| --- | --- |
| `nomodule/0` | Never loaded. |

## library(renamed)

`renamed.pl` loads, but it declares the module `other`.

<!-- predicates: renamed -->
| Predicate | Meaning |
| --- | --- |
| `renamed/0` | Exported by `other`, not by `renamed`. |
