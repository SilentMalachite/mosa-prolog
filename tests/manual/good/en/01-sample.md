# Sample chapter

A fixture for `mosatest --manual` (`manual_fixture_good` in `tests/tests.cmake`). Every example here passes.

A query and its answer. The input is the text after `?- `.

<!-- check: first-query -->
```text
?- X = 1.
X = 1.
```

More answers: the input is written in the marker, one line per key the reader types.

<!-- check: more-answers
(X = 1 ; X = 2 ; X = 3).
;

-->
```text
?- (X = 1 ; X = 2 ; X = 3).
X = 1;

X = 2
.
```

An error message comes out in the language of the edition.

<!-- check: error-message -->
```text
?- X is 1/0.
Warning: goal raised exception: error(evaluation_error(zero_divisor),context((is)/2,_G1))
```

A file, then commands that read it from the chapter directory. `mosatest` is the one beside `mosa`; it prints
nothing when the file reads back the same.

<!-- file: family-file family.pl -->
```prolog
parent(tom, bob).
parent(tom, liz).
```

<!-- check: two-commands -->
```console
$ mosa family.pl -g "parent(tom, liz)" -t halt
true.
$ mosa -g "write('a b'), nl" -t halt
a b
true.
$ mosatest --roundtrip family.pl
```

<!-- check: console-session -->
```console
$ mosa family.pl
?- parent(X, liz).
X = tom.
?- halt.
```

Not checked: it depends on the reader's machine.

<!-- skip: install -->
```console
$ brew install something
```
