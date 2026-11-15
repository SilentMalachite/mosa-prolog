English | [日本語](0027-conditional-compilation.ja.md)

Status: Approved (2026-11-16, owner; the owner's request for the phase 13 plan counts as approval). SPEC section 9 gives phase 13 (conditional compilation and libraries); this ADR fixes how the loader handles `:- if/elif/else/endif` and how `mosac` evaluates the conditions, the first user goals that a translation load calls.

## Context

The translation load of `mosac` runs declarations only and does not call a general directive `:- G`; at start-up the executable reads its embedded sources again with the same loader and attaches translated code only to the static predicates whose `(module, name, arity, fingerprint)` matches (ADR 0009).
`:- if` needs its branch decided at build time to choose the clauses to translate and the files to bundle, and it is the first path on which the translation load can read more than the load at start-up.
Two places rely on the load at start-up reading all that the translation load read: the executable installs a unit for each program module file that the translation load read, reading the file on its own if start-up has not, and the generated code binds in place a `M:G` into any module known after the translation load.

## Decision

The loader handles `if/elif/else/endif` per file, and a condition is solved once in the module being loaded, as `:- G` is. The translation load calls the conditions too, the only user goals it calls, and there an exception or a `halt` only means that the branch is not taken; libraries named literally in skipped terms are bundled when they exist; the executable's load at start-up evaluates the conditions again.
A module file that the load at start-up did not read gets no unit and is not read on its own; a `M:G` into a module first read inside a branch is not bound in place; a directive in a branch not taken counts as a skipped goal; an undefined export in a file with a branch not taken is a warning, not a stop.

## Rejected alternatives

Fixing the result at build time in the executable, as the C preprocessor does: `argv`, the environment, the files present and a skipped `:- G` would make the executable's result differ from `mosa`'s (iso-notes question 1, B).
Not evaluating the conditions at build time: the translator could not decide which files to bundle, and a dialect branch such as `:- if(Dialect == swi). :- use_module(library(clpfd)). :- endif.` would stop the build.

## Consequences

The conditions also run at build time. When the branches differ, the answer is still `mosa`'s, only slower, since the predicates that do not match run as IR; a file named by a plain path in a branch not taken at build time is read from the working directory at run time.
Task 3 of the phase 13 plan puts the conditions into the loader and Task 4 into `mosac`; iso-notes "Conditional compilation (not in 13211-1)" records the three questions and their decisions.
Tests: `tests/pl/cond_compile.pl` with files in `tests/pl/fixtures/cond_*.pl`, `tests/cpp/cond_compile_test.cpp`, the `mosac build` ctests of SPEC section 9 row 13, `codegen`, and `mosatest --diff`.
