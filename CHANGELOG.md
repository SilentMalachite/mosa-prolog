English | [日本語](CHANGELOG.ja.md)

# Changelog

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
