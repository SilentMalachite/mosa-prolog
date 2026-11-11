English | [日本語](CHANGELOG.ja.md)

# Changelog

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
