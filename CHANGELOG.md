English | [日本語](CHANGELOG.ja.md)

# Changelog

## Unreleased

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
