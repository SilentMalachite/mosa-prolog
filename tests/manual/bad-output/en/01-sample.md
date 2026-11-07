# Wrong output

A fixture for `mosatest --manual` (`manual_fixture_bad_output`). The answer shown below is wrong on purpose, so the
check fails and names the id `wrong-answer`.

<!-- check: right-answer -->
```text
?- X = 1.
X = 1.
```

<!-- check: wrong-answer -->
```text
?- X = 1.
X = 2.
```
