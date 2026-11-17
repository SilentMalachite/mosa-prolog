English | [日本語](CHANGELOG.ja.md)

# Changelog

## Unreleased

### Changed

- A goal built at run time whose control structure leads back to itself, as in `X = (true, X), call(X)`,
  `X = (fail ; X)`, `X = (X, true)`, `X = call(X)` or `X = user:X`, now raises `resource_error(cyclic_term)` before
  any part of it runs, as is/2 does for a cyclic expression. Mosa 0.4.0 ran such a goal as its infinite unfolding:
  `X = (a, X)` called `a`, `X = (Y = 1 ; X)` gave `Y = 1` on each retry, some goals ran without end, and others
  stopped at the heap limit, which `catch/3` could not receive. This holds for `call/N`, `\+/1`, `once/1`, a variable
  goal, `M:G`, `findall/3` and the other predicates that take a goal, for the `M:` layers that `call/N`, `phrase/2,3`
  and `setup_call_cleanup/3` take off, and for a cyclic grammar body given to `phrase/2,3`, which 0.4.0 also unfolded.
  A goal with a part that is not callable still raises `type_error(callable, Goal)` first. A goal whose arguments
  alone are cyclic, such as `X = f(X), call(p(X))`, is still an ordinary call, and a cyclic body given to `assertz/1`
  is still cut as before.

### Fixed

- `mosatest F` with a test file whose load stops at `halt/0,1` now writes the reason, as in
  `F: the load stopped at halt(3)`, and still exits with 1; Mosa 0.4.0 wrote only `F: `. `mosatest --diff` now
  reports such a `.pl` or `.lgt` file as one it cannot assemble and counts a difference, and `mosac emit --units`
  writes `mosac: F is not translated: the load stopped at halt(3)` and makes no unit for it. Mosa 0.4.0 ran none of
  the file's tests and counted it as agreeing.
- `strip_module/3`, `import/1` and the clause operations (`assertz/1`, `asserta/1`, `retract/1`, `retractall/1`,
  `clause/2`, `abolish/1`) and `predicate_property/2` now raise `resource_error(cyclic_term)` when the `M:` layers of
  their argument form a cycle, as in `X = a:X, strip_module(X, M, P)`, as is/2 does for a cyclic expression. Mosa 0.4.0
  looped there without end, and `--heap-max` did not stop it. In the clause operations and `predicate_property/2` a
  layer that names a module that does not exist still raises `existence_error(module, M)` first.
- `bagof/3` and `setof/3` now raise `resource_error(cyclic_term)` when the leading `^` layers of their goal form
  a cycle, as in `X = (_^X), bagof(a, X, L)`. Mosa 0.4.0 looped there without end, and `--heap-max` did not stop it.

### Known limits

- A cycle made only of the leaves `call/N`, `\+/1`, `once/1` and `M:G`, with no `,` `;` `->` `*->` or `catch/3` in it,
  is found only when its period is at most 32; a longer one runs as in Mosa 0.4.0.

## 0.4.0 (2026-11-17)

The release of phase 13 (conditional compilation and libraries): the directives `:- if(Goal)`, `:- elif(Goal)`,
`:- else` and `:- endif` work on every path that reads a text; `mosac build` evaluates their conditions at build time
and the executable evaluates them again at start-up, so it takes the same branches as `mosa`; and the libraries
`pairs`, `assoc` and `ordsets` are added. It runs on the same computers as 0.3.0
([chapter 2](docs/manual/en/02-install.md)), and every example of the [manual](docs/manual/en/README.md) is checked
against it.

### Added

- The conditional compilation directives `:- if(Goal)`, `:- elif(Goal)`, `:- else` and `:- endif`, which are not in
  ISO/IEC 13211-1 and follow the SWI-Prolog manual (4.3.1.2 Conditional compilation). They work on every path that
  reads a text: the command line, `consult/1`, `[File]`, `use_module/1,2` and `ensure_loaded/1` as directives and as
  goals, `include/1`, the libraries, `Machine::load_program` and the load at start-up of an executable built by
  `mosac build`. A condition is solved once in the module being loaded, as a directive `:- G` is, and one that
  raises an error fails the load, as such a directive does (SWI-Prolog prints the error and takes the condition as
  false). A skipped part is read term by term, so that the blocks nested in it pair up, but none of its directives
  runs and none of its clauses is added, and a syntax error in it is passed over without a message. Each `if` is
  closed by an `endif` in the same file; an `elif`, `else` or `endif` with no open `if`, an `elif` after `else`, a
  second `else` and an `if` left open fail the load, also inside a skipped part. Chapter 14 of the
  [manual](docs/manual/en/14-reference.md) describes them.
- `mosac build` evaluates the conditions at build time, to choose the clauses to translate and the files to bundle,
  and the executable evaluates them again at start-up, so it takes the branches that `mosa` would take. When the two
  take different branches, the result is still `mosa`'s, only slower, since the predicates that do not match run as
  IR. A condition that raises an error or calls `halt/0,1` at build time only means that its branch is not taken
  there. A library named literally in a `use_module` or `reexport` of any branch, or of a condition, is bundled when
  it exists. An exported predicate with no definition is a warning, not an error, in a module whose file has a branch
  not taken at build time. A directive or clause that fails in a branch taken at build time, or a syntax error there,
  does not stop the build when a goal was skipped before that branch's condition, since the branch may not be taken
  at start-up: the file runs as IR and the load at start-up decides. After such a block, a clause that cannot be
  added because of what its branch did, such as a clause apart from the earlier clauses of its predicate, does not
  stop the build either. Chapter 13 of the [manual](docs/manual/en/13-mosac.md) describes this.
- The libraries `library(pairs)` (6 predicates, such as `pairs_keys_values/3` and `group_pairs_by_key/2`),
  `library(assoc)` (18 predicates on an AVL tree, such as `list_to_assoc/2`, `get_assoc/3` and `put_assoc/4`) and
  `library(ordsets)` (20 predicates, such as `ord_union/3`, `ord_subtract/3` and `ord_memberchk/2`). They are written
  anew, and their names, argument order and documented errors follow the SWI-Prolog manual. `del_min_assoc/4` and
  `del_max_assoc/4` fail on an empty assoc, where the SWI-Prolog manual notes that they succeed without binding
  anything. Chapter 14 of the manual lists every predicate, and `mosac build` bundles them as it does the other
  libraries.
- For developers, the library column of `mosatest --suite` counts the 39 cases of the suite directory
  `directives/conditional_compilation`, and all of them pass: 600 of the 605 cases of that column pass. The ISO
  column stays at 2291 of 2326.

### Changed

- `:- if(G)`, `:- elif(G)`, `:- else` and `:- endif` are now conditional directives, and no longer call a predicate
  `if/1`, `elif/1`, `else/0` or `endif/0` that the program defines, as Mosa 0.3.0 did. Other arities are general
  directives as before, so `:- if(C, T, E)` still calls the built-in `if/3`. The four are not built-in goals: when
  the program defines no `if/1`, `?- if(true).` at the prompt still raises `existence_error(procedure, if/1)`. A
  program's own `if/1` can still be called as a goal; only the directive form `:- if(...)` is taken by the loader.

### Known limits

The known limits of 0.3.0 still hold. Five come with conditional compilation under `mosac`:

- A file named by a plain path (`include(foo)`, `use_module(foo)`, `ensure_loaded(foo)`) inside a branch not taken
  at build time is not bundled. If the executable takes that branch at start-up, it reads the file from the working
  directory, as it does for a file that a goal load names by a plain path.
- The conditions run at build time too, so a condition that writes output writes it in the output of `mosac build`
  as well.
- A call to a predicate that only a branch not taken at build time defines gets the undefined-call warning of
  `mosac`.
- The conditions are solved where `mosac build` runs. A condition that looks at the files there or at the flag
  `argv` can take a branch that the executable would not take where it runs, and a directive or clause that fails
  in that branch stops the build, as it would stop `mosa` in the same place, unless the rule above leaves it to
  start-up.
- A condition that calls `halt/0,1` stops `mosa` and the executable at start-up, but at build time it only means that
  its branch is not taken, so a directive or clause after it that fails can stop the build, although the program
  halts before reaching it.

## 0.3.0 (2026-11-16)

The release of phase 12 (module completion): `use_module/1,2` and `ensure_loaded/1` can be called as goals, at the
prompt too, and a goal load that fails writes its reason once; the directives `reexport/1,2` and the built-in
predicates `strip_module/3`, `context_module/1`, `module_property/2` and `import/1` are added; `mosac` bundles the
libraries that goals name literally; and a load that stops at the heap limit is undone as a failed load is. It runs on
the same computers as 0.2.2 ([chapter 2](docs/manual/en/02-install.md)), and every example of the
[manual](docs/manual/en/README.md) is checked against it.

### Added

- `use_module/1`, `use_module/2` and `ensure_loaded/1` can also be called as goals, at the `?-` prompt and in the
  body of a clause. Each means the same as the directive of the same form, and a file is still read only once. The
  imports go into the module of the goal: `user` at the prompt, `m` in a clause of module `m`. A relative name is
  opened from the working directory, also when the goal runs while a file is being loaded. An error in the arguments
  is thrown with the directive's error, such as `type_error(atom, _)`; any other failure writes the reason to
  `user_error` (see Changed) and the goal fails.
- The directives `reexport/1` and `reexport/2`. `:- reexport(File).` loads and imports a module as `use_module/1`
  does, and adds what it imports to the export list of the module being loaded. `:- reexport(File, Imports).` does
  that for the predicates in the list, and a predicate renamed with `as` is exported under its new name.
  `reexport/1` and `except(List)` pass on the exported operators too. A module that imports a re-exported predicate
  imports it from the module that defines it, so `predicate_property(P, imported_from(M))` names that module, and the
  same predicate reached along two paths is not an import conflict.
- The built-in predicates `strip_module/3`, `context_module/1`, `module_property/2` and `import/1`.
  `strip_module(Term, M, Plain)` takes the leading `M:` off a term; `context_module(M)` gives the module of the goal;
  `module_property(M, P)` gives the properties `file(F)`, `exports(L)` and `exported_operators(L)` of a module made by
  `module/2`; and `import(M:Name/Arity)` imports one predicate that `M` exports. The manual shows them and
  `reexport` in [chapter 12](docs/manual/en/12-modules-libraries.md) and lists them in chapter 14. The errors they
  raise are in section 5 of the [specification](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.md)
  in the source tree.
- `mosac` also bundles the libraries that a program names literally as `use_module(library(Name))` or
  `use_module(library(Name), Imports)` in a goal: in the body of a clause, in a directive `:- G` or in an
  `initialization/1` goal. The executable then finds them with no `$MOSA_LIB` and no `lib/` beside it. A library whose
  name is a variable is still looked for at run time, in `$MOSA_LIB` and then in `lib/`.

### Changed

- Six names enter the module `system`: `use_module/1,2`, `ensure_loaded/1`, `strip_module/3`, `context_module/1`,
  `module_property/2` and `import/1` are now built-in predicates. A clause for one of them, in any module, now gives
  `permission_error(modify, static_procedure, PI)`. Mosa 0.2.2 let a program define them, for example `user:import/1`.
- A goal load that fails writes its reason to `user_error` at the moment it fails, once, also when `;`, `findall/3`
  or `\+` absorbs the failure; this covers `consult/1`, `use_module/1,2` and `ensure_loaded/1` called as goals.
  Mosa 0.2.2 wrote the reason only when the whole query failed, and lost it when the failure was absorbed. The text
  is the one `mosa F` prints, such as `F: reason`, and when the whole query then fails, the toplevel does not write
  it again. In a program that embeds Mosa, the line goes to standard error, and `Machine::diagnostic()` still returns
  it.
- `ensure_loaded/1` called as a goal loads the file, or writes the reason and fails. Mosa 0.2.2 raised
  `existence_error(procedure, ensure_loaded/1)`.
- For developers, five expected values of the tests changed with these rulings (iso-notes "7.4.2 ensure_loaded/1"
  and "7.4.2 A goal load that fails: the diagnostic"): `ensure_goal` in `tests/pl/directives.pl` and `ensure_ha` in
  `tests/pl/japanese.pl` now expect `fail`; `check_ensure_goal` in `tests/cpp/ensure_test.cpp` expects the failure and
  its diagnostic; the check of the built-in list in `tests/cpp/manual_lists_test.cpp` expects the three load
  predicates among the built-ins; and the case "rejected load drops main" of `tests/cpp/main_goal_test.cpp` expects
  the diagnostic of the rejected load on standard error, where it expected no output.

### Fixed

- A directive that calls `consult/1` and absorbs its failure, as `:- (consult(F) -> true ; true).` does, no longer
  makes a later directive `:- G` fail the whole load with the absorbed reason, and a later `set_prolog_flag/2`
  directive that fails no longer reports the line and column of a syntax error in the absorbed file.
- In an executable built by `mosac`, a call `M:G` into a library that the program does not load, such as
  `lists:append(...)` or `format:format(...)`, now raises `existence_error(module, M)`, as `mosa` does. Mosa 0.2.2
  raised `existence_error(procedure, M:Name/Arity)` there.
- When a text read by the C++ embedding API's `Machine::consult` or `Machine::load_program` stops with `halt/0,1`, the
  call returns false with an empty `diagnostic()` (and an empty `LoadStatus` message), since a halt is not a failure.
  Mosa 0.2.2 left the path followed by `: ` there, or the text of a failure that a directive had absorbed. A goal load
  that halts also leaves `diagnostic()` empty.
- A load that stops at the heap limit, which the C++ embedding API passes on as an exception and the toplevel
  reports (at the prompt, before it goes on), is now undone as a failed load is. No half-read module, clause or mark of a
  loaded file is left, so a later `use_module` of the same file reads it again in full instead of using the
  half-read module. Later clauses and imports go where they went before the load, not into the module of the goal
  that started it. A query that was open while a directive stopped this way keeps its choice points, so `next()`
  gives its next answer. A file whose `initialization/1` goal stops this way keeps what it read, as when that goal
  raises an error, unless a load that is still reading its text encloses it: the exception leaves that load's
  reading, so that load is undone with every file it read. The goal of a file loaded by `:- use_module`,
  `:- ensure_loaded` or `:- reexport` runs after the loading file has been read, so both files are kept, unless the
  loading file was itself loaded by a file still being read (by `:- consult(File)`, for example). The goal of a file
  loaded by `:- consult(File)`, `:- [File]` or a load called as a goal inside another directive runs while the
  loading file is still being read, so the loading file is undone as a whole. After a load stops this way,
  `Machine::diagnostic()` is empty, since the exception carries the reason. Mosa 0.2.2 did none of this for
  `consult/1` called as a goal or for a load from a directive, and could leave in `diagnostic()` the text of a
  failure that a directive had absorbed.
- A directive typed at the prompt that stops at the heap limit is now reported, and the prompt goes on, as for a
  goal. Mosa 0.2.2 ended the session there with exit status 2.

### Known limits

The known limits of 0.2.2 still hold. Five come with the module changes:

- The reason that a failed goal load writes goes past the `user_error` stream, as Mosa's warnings do, so the column
  that stream counts does not move.
- In an executable built by `mosac build`, `module_property(M, file(F))` gives the file's embed key resolved against
  the working directory: `library/Name.pl` for a bundled library, and the path relative to the main file's directory
  for the program's own module files. It can differ from the path that the interpreter gives.
- After `use_module(library(apply))` as a goal, `mosac` still warns that an unqualified call such as `maplist/3` is
  undefined, although the executable runs correctly: the translator reads a library named in a goal without importing
  it anywhere, as `use_module(library(apply), [])`.
- A call `M:G` into a module that the program does not load by a directive (one that it loads only from a goal, or a
  library such as `lists` that it does not load at all) goes through `:/2` at run time in an executable built by
  `mosac`, and is not bound in place. The result is the same as in `mosa`; the call is only slower.
- `mosac` does not bundle a file that a goal load names by a plain path, such as `use_module(helper)` in the body of
  a clause: the executable reads it relative to the working directory when the goal runs. Of the files named in
  goals, only the libraries named literally as `library(Name)` are bundled.

## 0.2.2 (2026-11-14)

The release of phase 11 (conformance scope and `format`): `format/1,2,3` gains the directives `~i`, `~k`, `~W`, `~p`,
`~N`, `~r`, `~R`, `~D`, `~E`, `~g`, `~G` and `~+`; `numbervars/3` raises `type_error(integer, End)` when End is
neither an integer nor a variable; the ISO column of the conformance suite gains `sort_2`, `keysort_2`, `ground_1` and
`not_unifiable_2`, the conformance runner reads each suite directory's helper predicates, and the report counts the
library predicates in a column of their own, apart from the ISO rate. It runs on the same computers as 0.2.1
([chapter 2](docs/manual/en/02-install.md)), and every example of the [manual](docs/manual/en/README.md) is checked
against it.

### Added

- `format/1,2,3` (`library(format)`) has twelve more directives, the ones the bundled conformance suite tests. `~p`
  writes an argument as `print/1`, `~k` as `write_canonical/1`, and `~W` takes a term and a list of write options;
  `~i` skips an argument; `~N` writes a new line only when the output is not already at the start of a line; `~r`
  and `~R` write an integer in a radix from 2 to 36 (8 without a number), with small or capital letters, as in
  `~16r` giving `ff`; `~D` writes an integer with a `,` between each group of three digits, as in `1,234,567`; `~E`
  writes a float as `~e` does, with a capital `E`; `~g` and `~G` write a float as the C formats `%g` and `%G` do;
  and `~+` sets a column a number of columns after the previous column stop (8 without a number). `~+` counts a wide
  East Asian character as 2 columns, as `~t~|` does. A radix outside 2 to 36 raises `domain_error(radix, N)`.
- For developers, the conformance report `docs/conformance.md` has a second column, the library column. It counts the
  13 suite directories of the library predicates Mosa provides outside ISO/IEC 13211-1, such as `format_2`,
  `between_3` and `length_2`, and never enters the ISO rate. It has its own pass rate, its own badge data
  `docs/conformance-library.json` and a second badge in the README, and `mosatest --suite` also fails when that rate
  goes down (the 90% floor is for the ISO rate only). 561 of the 566 cases pass; `docs/iso-notes.md` gives the reason
  for each of the five left.

### Changed

- `numbervars/3` raises `type_error(integer, End)` when End is neither an integer nor a variable, as the bundled
  conformance suite expects. Mosa 0.2.1 failed there.
- For developers, the ISO column of the conformance suite now also counts `sort_2`, `keysort_2`, `ground_1` and
  `not_unifiable_2`, the tests of ISO predicates (8.4.3, 8.4.4, 8.3.10 and 8.2.3) that were left out of it: 2291 of
  2326 cases pass (98.5%), where 2239 of 2274 passed in 0.2.1. The conformance runner now reads the clauses and the
  `dynamic` and `multifile` directives of each directory's `tester.lgt`, which hold the helper predicates the tests
  call, and a binding a test body makes inside `{}` now reaches the goals after it.
- For developers, `mosatest --suite` now also fails when a column ran no tests although a badge JSON with a rate from
  an earlier run exists, and keeps that JSON. It used to overwrite the JSON with `no tests`, losing the rate the next
  run compares against, and for the library column, which has no 90% floor, it exited 0. A column with no earlier
  rate still gets the `no tests` badge.

### Known limits

The known limits of 0.2.0 still hold. Two come with the new `format` directives:

- At the `?-` prompt, Mosa writes the answer and the next prompt past `user_output`, so the line position that `~N`
  reads stays where the query's own output left it. After a query whose output does not end with a new line, as
  `?- write(a).`, a `~N` at the start of the next query's output writes an empty line.
- `~r` and `~R` build the digits of a big integer as text before they write it, and that text is memory the memory
  limit does not count. So writing a big integer near the limit in radix 2 takes several times its size in memory
  outside the limit, as `~d` does.

## 0.2.1 (2026-11-13)

A maintenance release after a review of all of Mosa and a follow-up review: file reading no longer loads a whole
file at open, the read-only `iso` flag is implemented, several quadratic costs are gone, and `mosatest` no longer
passes empty or halted tests. It runs on the same computers as 0.2.0 ([chapter 2](docs/manual/en/02-install.md)),
and every example of the [manual](docs/manual/en/README.md) is checked against it.

### Added

- The read-only flag `iso` is `true`. Setting it to `true` or `false` gives `permission_error(modify, flag, iso)`;
  another value gives `domain_error(flag_value, iso+Value)`. The flag states that the core semantics follow ISO;
  there is no separate strict-conformance mode.

### Changed

- A file opened for reading is read in chunks as they are needed instead of all at once when it is opened. The bytes
  kept by all open streams count against the memory limit together with the heap each time more is read, and a
  query stops at that limit; bytes read in the stopping chunk stay in the stream, so a later query does not skip
  them. Text appended by another program after a peek saw the end of the file can now be read. Loading a Prolog
  text, and reading what is already in a file opened for append, accept regular files only under the same limit.
- `mosatest` is stricter about empty and stopped tests: an own-test file with no `test/3` facts, and a folder with
  no `.pl` files, now end with status 1. In the conformance-suite runner, a test that halts is a failure whatever
  outcome it expected, and the report shows it as `halt(N)`. `mosatest --suite` also fails, without rewriting the
  badge JSON, when the pass rate falls below the rate in the JSON already in the tree.
- For developers, the CTest presets now run tests in parallel with a default per-test timeout. CI also has a Linux
  Debug job built with AddressSanitizer and UndefinedBehaviorSanitizer.

### Fixed

- `=..`/2 on long or cyclic lists, reading a term with many different variables, and writing an answer with many
  named or unnamed variables no longer take time quadratic in their size.
- `docs/conformance.md` no longer embeds absolute paths from the machine that generated it.
- `set_stream_position/2` counts a character split across an internally read chunk once, at its full width.

## 0.2.0 (2026-11-13)

A release that makes integers arbitrary-precision: an integer result that raised `evaluation_error(int_overflow)` in
0.1.4, and an integer in a text that raised `syntax_error(integer_overflow)`, now give the exact integer (see
Changed). It runs on the same computers as 0.1.4 ([chapter 2](docs/manual/en/02-install.md)), and every example of
the [manual](docs/manual/en/README.md) is checked against it.

### Added

- Integers have arbitrary precision: arithmetic, reading and writing keep an integer above 9223372036854775807 or
  below -9223372036854775808 exact, and the built-in predicates that take an integer accept one. The flag `bounded` is
  `false`. `max_integer` and `min_integer` stay 9223372036854775807 and -9223372036854775808.
- Time: arithmetic on a big integer takes longer as the integer grows. Computing `7^7^7`, an integer of 2,311,978
  bits, takes about 0.16 s, but turning it into decimal text with `number_codes/2` takes about 12 s, because that time
  grows with the square of the length. In the benchmarks `nrev` and `queens`, which use only 64-bit integers, the
  interpreter takes 0.9% and 3.1% longer than in 0.1.4, and the translated `queens` 8.1% longer (measured on one Mac,
  [bench.md](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.md#cost-of-arbitrary-precision-integers)).

### Changed

- An integer result no longer raises `evaluation_error(int_overflow)`, and an integer of any length reads without
  `syntax_error(integer_overflow)`, which no longer exists. The only limit on the size of an integer is the memory
  limit (`--heap-max`), and a result that does not fit in the memory left stops the query with the heap-limit
  message. `^` and `<<`, and the two-argument `+`, `-`, `*` and the bit operations (`/\`, `\/`, `xor` and `\`) with
  a big integer argument, estimate the size of the result first, and stop before computing it when the estimate is
  more than the memory left. The other operations, such as `//`, `mod`, `gcd/2`, `abs/1` and `-` of one argument,
  compute the result first and stop when it is stored. `evaluation_error(int_overflow)` is left only for rounding an
  infinite float to an integer, as in `X is round(1.0Inf)`.
- An integer beyond the range of a float raises `evaluation_error(float_overflow)` wherever evaluation must turn it
  into a float: with a float in `+`, `-`, `*`, `/` and `^`, in `**` and the float functions such as `sqrt/1`, `log/1`
  and `sin/1`, in an arithmetic comparison with a float (`<`, `=:=` and the others), in `min/2` and `max/2` with a
  float, and in `float/1`. `/` of two integers raises it only when the quotient is beyond that range: `2^2000 / 3`
  raises it, and `2^2000 / 2^1999` is `2.0`. The standard order (`compare/3`, `@<`) compares such an integer with a
  float without an error.
- `halt/1` with an integer outside -2147483648 to 2147483647 (a C++ `int`) raises `representation_error(max_integer)`,
  or `representation_error(min_integer)` when the integer is negative. Mosa 0.1.4 passed only the low bits on, so
  `halt(4294967296)` ended with status 0.
- `tab/1` and `tab/2` with a count above 9223372036854775807 write spaces without end, the same as a 64-bit count
  too large to finish. Mosa 0.1.4 raised `evaluation_error(int_overflow)` for such a count, as in `tab(2^70)`. A
  negative count of any size writes nothing.

### Known limits

The known limits of 0.1.4 still hold. Three come with big integers:

- In an arithmetic step with big integers, Mosa checks that the result fits in the memory left, and that check
  counts the memory of terms no longer used as taken. So such a step can stop at the memory limit although a garbage
  collection would have made room.
- Arithmetic works on copies of the big integers it reads, so the largest integer it can use is below the memory
  limit: reading one, as in `A > 0` or `A mod 7`, stops once the integer takes about half of the limit, and an
  operation with a result as large, as in `A + 0`, at about a third.
- An integer in a text that Mosa reads is turned into a number in full before it is compared with the memory limit.
  So reading a very long integer takes several times the memory of its text before it stops at the limit.

## 0.1.4 (2026-11-11)

A maintenance release after a review of all of Mosa's C++ code. A program sees no change from 0.1.3. It runs on the
same computers as 0.1.3 ([chapter 2](docs/manual/en/02-install.md)), and every example of the
[manual](docs/manual/en/README.md) is checked against it.

### Fixed

- Three UTF-8 decoders inside Mosa, two in the writer and one in the reader, took a surrogate (U+D800 to U+DFFF), an
  overlong form or a code point past U+10FFFF for one character. They now reject them, as the lexer does. No program
  reaches them today: `atom_codes/2`, `char_code/2` and reading a file reject such characters first.

### Known limits

The known limits of 0.1.3 still hold:

- `assertz/1` does not translate a grammar rule: translate it with `expand_term/2` of `library(dcg)` first, and assert
  the clause it returns. `term_expansion/2` and `goal_expansion/2` do not exist.
- `use_module/1` works only in a file: typed at the `?-` prompt, it gives a warning with
  `existence_error(procedure,use_module/1)` ([chapter 12](docs/manual/en/12-modules-libraries.md)).
- Typing Japanese in the console window of Windows (with `chcp 932` or `chcp 437`) has not been verified.
- `mosac build --debug` links the runtime library built with optimization, and the libraries are not traced.
- `mosac build` of a source clause nested 100,000 deep works, but on macOS clang took about 80 minutes and 1.2 GB of
  memory to compile it.
- On Windows, Mosa does not start while Smart App Control is on ([chapter 2](docs/manual/en/02-install.md)).

## 0.1.3 (2026-11-11)

A release that adds ISO built-ins and reads deeply nested terms. It runs on the same computers as 0.1.2
([chapter 2](docs/manual/en/02-install.md)), and every example of the [manual](docs/manual/en/README.md) is checked
against it.

### Added

- `subsumes_term/2` (8.2.4) and `acyclic_term/1` (8.3.11) are built in
  ([chapter 14](docs/manual/en/14-reference.md)). `subsumes_term/2` binds nothing.
  A program that defined its own `subsumes_term/2` or `acyclic_term/1` must drop that definition: loading it now gives
  `permission_error(modify,static_procedure,subsumes_term/2)` (or `acyclic_term/1`).
- `Name//Arity` names a non-terminal as `Name/(Arity+2)` in export and import lists, and in the `dynamic`,
  `discontiguous` and `multifile` directives. `abolish/1` stays ISO.
- `*->` in a grammar body is a soft cut.
- `library(dcg)` exports `expand_term/2`: it translates one grammar rule `H --> B` into a clause, and returns any
  other term that is not a variable unchanged ([chapter 12](docs/manual/en/12-modules-libraries.md)). Load it with
  `:- use_module(library(dcg)).`
- Mosa reads terms nested very deeply without crashing: terms 1,000,000 deep in arguments, lists, prefix operators,
  `xfy` operands, parentheses, curly braces and `|` tails, under a stack of 1 MiB.

### Fixed

- `read/1` and `read_term/2,3` on a piped `user_input` take time in proportion to the term's length, also when
  its quoted atoms, strings or comments hold many sentences. They read the term again from its start for each
  byte they added, so a 19 KB term took 8 seconds and a 60 KB one 81 seconds.

### Known limits

The known limits of 0.1.2 still hold, except the four that this release removes (a term nested very deeply crashing
Mosa, `subsumes_term/2` and `acyclic_term/1` missing, `Name//Arity` in an export list, `*->` in a grammar body):

- `assertz/1` does not translate a grammar rule: translate it with `expand_term/2` of `library(dcg)` first, and assert
  the clause it returns. `term_expansion/2` and `goal_expansion/2` do not exist.
- `use_module/1` works only in a file: typed at the `?-` prompt, it gives a warning with
  `existence_error(procedure,use_module/1)` ([chapter 12](docs/manual/en/12-modules-libraries.md)).
- Typing Japanese in the console window of Windows (with `chcp 932` or `chcp 437`) has not been verified.
- `mosac build --debug` links the runtime library built with optimization, and the libraries are not traced.
- `mosac build` of a source clause nested 100,000 deep works, but on macOS clang took about 80 minutes and 1.2 GB of
  memory to compile it.
- On Windows, Mosa does not start while Smart App Control is on ([chapter 2](docs/manual/en/02-install.md)).

## 0.1.2 (2026-11-11)

A release that adds grammar rules. It runs on the same computers as 0.1.1
([chapter 2](docs/manual/en/02-install.md)), and every example of the [manual](docs/manual/en/README.md) is checked
against it.

### Added

- Grammar rules: a clause `Head --> Body` in a file is translated to an ordinary clause when the file is loaded, in
  `mosa` and in programs built by `mosac`. `phrase/2` and `phrase/3` are built in
  ([chapter 14](docs/manual/en/14-reference.md)). In a grammar body, `{G}` calls `G` as `call/1` does, so a cut
  inside `{}` cuts only inside it.

### Known limits

The known limits of 0.1.1 still hold, except the one that this release removes (grammar rules). Grammar rules have
limits of their own, the first three below:

- A grammar rule passed to `assertz/1` is not translated, and `term_expansion/2`, `goal_expansion/2` and
  `expand_term/2` are not built in.
- An export list cannot name a non-terminal as `Name//Arity`: `:- module(m, [greet//0])` is a
  `type_error(predicate_indicator,greet//0)`. Write the predicate the rule becomes instead, with two more arguments:
  `greet/2`.
- `*->` in a grammar body is not a soft cut: `([x] *-> [y] ; [z])` calls a non-terminal `(*->)/4`.
- `subsumes_term/2` (8.2.4) and `acyclic_term/1` (8.3.11) are not built in.
- `use_module/1` works only in a file: typed at the `?-` prompt, it gives a warning with
  `existence_error(procedure,use_module/1)` ([chapter 12](docs/manual/en/12-modules-libraries.md)).
- Typing Japanese in the console window of Windows (with `chcp 932` or `chcp 437`) has not been verified.
- `mosac build --debug` links the runtime library built with optimization, and the libraries are not traced.
- A term nested very deeply in a file, such as `f(f(f(…)))`, can crash Mosa while it reads the file: from about 600
  levels on Windows, and from a few thousand on macOS and Linux.
- On Windows, Mosa does not start while Smart App Control is on ([chapter 2](docs/manual/en/02-install.md)).

## 0.1.1 (2026-11-10)

A release that adds `mosac dist` and fixes the bugs below. It runs on the same computers as 0.1.0
([chapter 2](docs/manual/en/02-install.md)), and every example of the [manual](docs/manual/en/README.md) is checked
against it.

### Added

- `mosac dist app.pl -o dist` builds the program and packs it for other people: one archive
  `app-<system>-<processor>.tar.gz` (`.zip` on Windows) with the executable, a `README.txt` in English and Japanese,
  and the `LICENSE` and `NOTICE` of the Mosa runtime that the executable contains
  ([chapter 13](docs/manual/en/13-mosac.md)). The archive is not compressed.

### Fixed

- `format/1`, `format/2` and `format/3` no longer leave a choice point. At the `?-` prompt, a query such as
  `format("~w~n", [a])` now ends with `true.`, and Mosa goes on to the next query. Mosa 0.1.0 showed `true` with no
  period and waited, as if there could be another answer. Each call also left one choice point behind, so a
  recursive loop that called `format` many times stopped at the heap limit.
- A bare `,` is no longer read as an atom: `f((,))` and `,(a, b)` are syntax errors, as ISO says (6.4); write `','`
  instead. A bare `|` is an atom only while `|` is an operator (6.3.4.3).
- `char_code/2` with both arguments given now compares them: `char_code(a, 98)` fails. Mosa 0.1.0 succeeded.
- `bagof/3` and `setof/3` no longer put a solution whose free variable is bound to a term such as `'$rename'(0)` in
  the same group as a solution where that variable is unbound.
- Closing a stream that reads a file no longer loses what another stream had written to the same file but not yet
  saved. After `open(F, write, W), write(W, hello), open(F, read, R), close(R)`, the file was empty when Mosa
  stopped.
- `call/2` and the other `call/N` no longer crash on a goal with very many module qualifiers, such as one built at
  run time with a million `user:`.
- `:- halt(N).` typed at the `?-` prompt now stops Mosa with status `N`. Mosa 0.1.0 ignored it and printed an empty
  warning.
- When a file is not loaded because of a syntax error, its `initialization(G, main)` no longer runs.
- A file with a syntax error again keeps the clauses that were there before it was loaded, also after a directive in
  it caught an error of a nested `consult/1`.
- `mosac build` and `mosac run` work for a predicate with 65,536 clauses or more. In Mosa 0.1.0 the generated C++
  did not compile. `mosac emit` now reports an error when a file it writes cannot be completed.
- `mosatest`: an expected `solutions(L)` passes only when the goal has exactly the solutions in `L`, in that order.
  Mosa 0.1.0 checked only the first ones, and passed a goal that succeeded for `solutions([])`. A test folder that
  cannot be read is now an error message, not a crash.

### Known limits

The known limits of 0.1.0 still hold, except the two that this release removes (`format` and `mosac dist`):

- Grammar rules written with `-->` are not supported: a file that holds one does not load.
- `subsumes_term/2` (8.2.4) and `acyclic_term/1` (8.3.11) are not built in.
- `use_module/1` works only in a file: typed at the `?-` prompt, it gives a warning with
  `existence_error(procedure,use_module/1)` ([chapter 12](docs/manual/en/12-modules-libraries.md)).
- Typing Japanese in the console window of Windows (with `chcp 932` or `chcp 437`) has not been verified.
- `mosac build --debug` links the runtime library built with optimization, and the libraries are not traced.
- A term nested very deeply in a file, such as `f(f(f(…)))`, can crash Mosa while it reads the file: from about 600
  levels on Windows, and from a few thousand on macOS and Linux.
- On Windows, Mosa does not start while Smart App Control is on ([chapter 2](docs/manual/en/02-install.md)).

## 0.1.0 (2026-11-08)

The first release of Mosa Prolog, a Prolog system that follows ISO/IEC 13211-1. The
[manual](docs/manual/en/README.md) shows everything below step by step, and every example in it is checked against
this release. Only installation steps and other things that depend on your machine are not run.

### What it does

- `mosa` runs Prolog: the `?-` prompt, files given on the command line, goals given with `-g`, and `-t halt` to quit
  after them. A file with `initialization(main, main)` runs `main` and exits with 0, 1 or 2, as `main` succeeds,
  fails or raises an error.
- `mosac` turns a Prolog program into an executable that runs without `mosa` and without the `.pl` files.
  `mosac build` writes the executable, `mosac run` builds it and runs it at once, `mosac emit` writes only the C++
  files and a `CMakeLists.txt`, and `mosac toolchain` shows the C++ compiler that `mosac` would use, or what to
  install. `mosac` needs a C++ compiler: the Xcode Command Line Tools on macOS, `g++` on Linux, Visual Studio 2022 on
  Windows
  ([chapter 13](docs/manual/en/13-mosac.md)).
- `mosatest` runs tests written as facts `test(Name, Goal, Expected)`, and checks the examples of the manual.
- Japanese: Mosa reads and writes UTF-8 and counts characters as Unicode code points. Atoms and variables can have
  Japanese names, full-width punctuation is read as half-width when the flag `japanese_punctuation` is `true`, and
  the messages are in Japanese when `LANG` starts with `ja`. On Windows without `LANG`, they follow the Windows
  display language ([chapter 11](docs/manual/en/11-japanese.md)).
- Memory: the heap grows as needed up to a limit, 4 GiB by default (`--heap-max`, `MOSA_HEAP_MAX`), and the garbage
  collector frees the terms that a program no longer uses. `garbage_collect/0` and `statistics/2` are built in.
- Libraries: `lists`, `apply`, `between`, `format`, `strings` and `japanese`, loaded with
  `:- use_module(library(Name)).` in a file ([chapter 12](docs/manual/en/12-modules-libraries.md)).
- Conformance: 2136 of the 2171 tests (98.4%) of the Logtalk conformance suite bundled with the source pass.

### Supported platforms

| Computer | Package |
| --- | --- |
| macOS on Apple Silicon | `mosa-0.1.0-macos-arm64.tar.gz` |
| Linux on x86_64 | `mosa-0.1.0-linux-x86_64.tar.gz` |
| Windows on x64 | `mosa-0.1.0-windows-x86_64.zip` |

- macOS 26 or later on Apple Silicon.
- Linux on x86_64 with the glibc of Ubuntu 24.04 or later.
- Windows 10 22H2 or later and Windows 11, on x64, with Smart App Control off.

The packages are not signed. [Chapter 2](docs/manual/en/02-install.md) shows how to start Mosa anyway on macOS and
Windows.

### Known limits

- Grammar rules written with `-->` are not supported: a file that holds one does not load.
- `subsumes_term/2` (8.2.4) and `acyclic_term/1` (8.3.11) are not built in.
- `use_module/1` works only in a file: typed at the `?-` prompt, it gives a warning with
  `existence_error(procedure,use_module/1)` ([chapter 12](docs/manual/en/12-modules-libraries.md)).
- After a query with `format/1`, `format/2` or `format/3` at the `?-` prompt, Mosa shows `true` with no period and
  waits, as if there could be another answer ([chapter 3](docs/manual/en/03-first-query.md)). Press Enter, and Mosa
  prints the period; `;` gives `false.`.
- Typing Japanese in the console window of Windows (with `chcp 932` or `chcp 437`) has not been verified.
- `mosac dist` is not part of Mosa 0.1.0.
- `mosac build --debug` links the runtime library built with optimization, and the libraries are not traced.
- A term nested very deeply in a file, such as `f(f(f(…)))`, can crash Mosa while it reads the file: from about 600
  levels on Windows, and from a few thousand on macOS and Linux.
- On Windows, Mosa does not start while Smart App Control is on ([chapter 2](docs/manual/en/02-install.md)).

### Differences from ISO

Mosa follows ISO/IEC 13211-1 and its corrigenda. Where it does not, or where ISO leaves the choice to each system:

- Integers are 64-bit: there are no larger integers, and going past `max_integer` raises
  `evaluation_error(int_overflow)`. Floats are 64-bit, and Mosa writes the shortest text that reads back as the same
  float.
- `unknown` takes only `error` and `fail`, and the three `float_*` flags take only `error`.
- Modules use the syntax of part 2 of the standard (ISO/IEC 13211-2), and a module holds predicates and operators.
  Atoms are the same in every module.
- `format/2`, `between/3`, `member/2`, `append/3` and the other predicates of the libraries are not built in, and
  need `use_module`.
- Full-width punctuation is read as half-width only when the flag `japanese_punctuation` is `true`.
- `subsumes_term/2` and `acyclic_term/1` are missing (see the known limits above).

[Chapter 14](docs/manual/en/14-reference.md) lists these with the built-in predicates, and marks those that are not
in ISO. The reasons for these choices, clause by clause, are in the
[ISO interpretation notes](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/iso-notes.md) of the
source tree.
