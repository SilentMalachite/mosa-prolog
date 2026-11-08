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

Without `-t halt`, `mosa` stays at the prompt after the goal, and with no input the prompt is left waiting. The checker
does not remove that prompt, because the reader sees it too, so this example fails.

<!-- check: waiting-prompt -->
```console
$ mosa -g "write(x), nl"
x
true.
```
