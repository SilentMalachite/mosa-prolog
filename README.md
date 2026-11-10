English | [日本語](README.ja.md)

# Mosa Prolog

[![CI](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml)
[![ISO conformance](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fconformance.json)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.md)
[![mosac diff](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fagreement.json)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.md)
[![Phase](https://img.shields.io/badge/phase-8_done-blue)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/STATUS.md)
[![Version](https://img.shields.io/badge/version-0.1.2-blue)](CHANGELOG.md)
[![ISO/IEC 13211-1](https://img.shields.io/badge/ISO%2FIEC-13211--1-informational)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.md)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://github.com/SilentMalachite/mosa-prolog/blob/main/CMakeLists.txt)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](https://github.com/SilentMalachite/mosa-prolog/blob/main/CMakePresets.json)
[![Platforms](https://img.shields.io/badge/platform-macOS_%7C_Linux_%7C_Windows-lightgrey)](https://github.com/SilentMalachite/mosa-prolog/blob/main/.github/workflows/ci.yml)
[![Unicode](https://img.shields.io/badge/Unicode-18.0.0-orange)](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/unicode-version.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

Mosa Prolog is a Prolog system that follows ISO/IEC 13211-1. It has three commands: `mosa` runs Prolog at the `?-` prompt or from files, `mosac` turns a Prolog program into an executable that runs without `mosa`, and `mosatest` runs tests. Mosa reads and writes UTF-8, so atoms and variables can have Japanese names, and its messages can be in Japanese. It is written in C++20 and depends only on the C++20 standard library and CMake.

## Download

Mosa Prolog 0.1.2 is on the [Releases page](https://github.com/SilentMalachite/mosa-prolog/releases). Download the package for your computer:

| Computer | Package |
| --- | --- |
| macOS on Apple Silicon | `mosa-0.1.2-macos-arm64.tar.gz` |
| Linux on x86_64 | `mosa-0.1.2-linux-x86_64.tar.gz` |
| Windows on x64 | `mosa-0.1.2-windows-x86_64.zip` |

The packages run on:

- macOS 26 or later on Apple Silicon.
- Linux on x86_64 with the glibc of Ubuntu 24.04 or later.
- Windows 10 22H2 or later and Windows 11, on x64, with Smart App Control off.

The packages are not signed. [Chapter 2 of the manual](docs/manual/en/02-install.md) shows how to unpack a package, add its `bin` folder to your PATH, and start Mosa anyway on macOS and Windows. `mosa` needs nothing else. Only `mosac` needs a C++ compiler ([chapter 13](docs/manual/en/13-mosac.md)).

## First steps

Type `mosa` in your shell. Mosa shows the prompt `?- ` and waits. Type each query after `?- ` and press Enter; these three are from [chapter 3](docs/manual/en/03-first-query.md):

```console
$ mosa
?- X = hello.
X = hello.
?- atom_length(hello, N).
N = 5.
?- halt.
```

`X = hello.` gives the variable `X` the value `hello`, `atom_length(hello, N).` counts the characters of `hello`, and `halt.` quits. Every query ends with a period.

## Manual

The [manual](docs/manual/en/README.md) ([日本語](docs/manual/ja/README.md)) is written for people who have never used Prolog. It starts with installing Mosa and goes on to writing programs and building executables with `mosac`. Chapter 14 is a reference of the commands, flags and built-in predicates. Every example in the manual is checked against Mosa 0.1.2; only installation steps and other things that depend on your machine are not run. What each release has, and its known limits, are in the [CHANGELOG](CHANGELOG.md).

## Build from source

You need this only to build Mosa yourself, for example to work on it. The packages above are already built.

### Requirements

- Git, to get the source
- CMake 3.25 or later, and Ninja (macOS and Linux)
- One of these compilers:
  - macOS 26 or later: Xcode 26 or later, or its Command Line Tools (Apple Clang 17)
  - Linux: GCC 13 or later, or Clang 17 or later
  - Windows: Visual Studio 2022 (MSVC v17.8 or later)

On macOS, `xcode-select --install` installs the Command Line Tools, and [Homebrew](https://brew.sh) installs the rest with `brew install cmake ninja`. On Ubuntu 24.04, `sudo apt update && sudo apt install git g++ cmake ninja-build` installs everything.

### Build and test

Get the source:

```sh
git clone https://github.com/SilentMalachite/mosa-prolog.git
cd mosa-prolog
```

Pick the preset for your OS: `macos-clang`, `linux-gcc`, `linux-clang`, or `windows-msvc`. On macOS:

```sh
cmake --preset macos-clang
cmake --build --preset macos-clang
ctest --preset macos-clang
```

Executables go to `build/` (`build/Release/` on Windows). `ctest --preset` runs the tests one at a time, which takes several minutes. Do not add `-j`: two of the tests share a scratch folder and can fail when they run at the same time.

## Name

Mosa stands for **M**odular **O**ptimized **S**ymbolic **A**rchitecture. Mosa Prolog is short for Modular Optimized Symbolic Architecture Prolog.

## Status

Phases 0–8 are done. Phase 8 (grammar rules: `-->` and `phrase/2,3`) was finished on 2026-11-11 after the check of its exit criteria: `tests/pl/dcg.pl` passes in `mosa` with zero differences under `mosatest --diff`, and the conformance pass rate did not go down (CI on macOS, Linux and Windows). Phase 7 (the GC, the iterative writer and loader, the manuals, the release packages and `mosac dist`) was finished on 2026-11-10 after the check of its exit criteria: memory plateaus in long-running loops ([`docs/bench.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.md#memory-in-long-loops)), and the steps in this README alone build Mosa and pass its tests (in CI on Linux and Windows, and from a fresh clone on macOS). Current progress is in [`docs/STATUS.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/STATUS.md).

| Phase | Scope | Status |
| --- | --- | --- |
| 0 | Skeleton (CMake, CI, test harness) | Done |
| 1 | Terms, reader, writer | Done |
| 2 | Execution core (IR, clause compiler, choice points, cut, `catch/throw`, REPL, `consult/1`) | Done |
| 3 | Built-in predicates, streams, flags, directives, error system | Done |
| 4 | Modules and `library(*)` | Done |
| 5 | Conformance push, full-width symbol mode, Japanese messages | Done |
| 6 | `mosac` (Prolog to C++20 translation and build), differential tests | Done |
| 7 | GC, manuals, `mosac dist`, release packages | Done |
| 8 | Grammar rules (`-->`, `phrase/2,3`) | Done |

The "mosac diff" badge is the agreement between the interpreter and the `mosac`-translated code that `mosatest --diff` measures over `tests/pl` and `tests/conformance` (`docs/agreement.md`, generated).

## Usage

### `mosa` (interpreter)

```sh
mosa [file...] [-g goal]... [-t halt] [--heap-max MiB] [-- argument...]
```

Started from a terminal without `-t halt`, `mosa` enters interactive mode once any `-g` goals have run. A `-g` goal may end with `.` or leave it out. A goal that cannot be read prints a syntax error; with `-t halt` the exit status is then 2. When an answer may have more solutions, type `;` and Enter for the next one; any other line, an empty one included, stops. The reply is read as a whole line, and its first character other than a space or a tab decides. The newline that `read/1` reads after `foo.` to see the end of the term is not taken as the reply; the rest of a line that a goal read only in part (with `get_char/1`, say) is. An executable built with `mosac build --interactive` asks the same way, also when its standard input is a pipe.

`-g` may be given more than once, and the goals run in order. With `-t halt`, or when standard input is not a terminal, the first goal that fails or raises an exception (or cannot be read) ends the run with status 1 or 2, and the later goals do not run; at a terminal every goal runs as if typed at the prompt, and interactive mode follows. With no `-g`, a run with `-t halt` or without a terminal exits 0 once the files have loaded, as an executable built by `mosac` does. Without a terminal, at least one file, `-g` or `-t halt` is needed; with none of them `mosa` prints the usage text and exits 1. Answer lines are written as each goal finishes, so they stay in order with the program's own output.

Everything after `--` is passed to the program as the read-only flag `argv`, a list of atoms (`[]` without `--`). A file with `:- initialization(main, main).` calls `main` once after loading and then exits: 0 on success, 1 on failure, 2 after printing an uncaught exception, and the `halt/1` value if `main` halts. It then neither enters interactive mode nor runs `-g`.

```console
$ ./build/mosa tests/pl/nrev.pl -g "nrev([1,2,3], R)." -t halt
R = [3,2,1].
$ ./build/mosa -g "atom_length('𠮷野家', N)." -t halt
N = 3.
```

A character is always one Unicode scalar value, so `𠮷` counts as one character.

#### Heap, GC and `statistics/2`

The heap grows on demand up to a limit and is garbage-collected (mark and compact, ADR 0010); the default limit is 4 GiB. `--heap-max <MiB>` sets it for `mosa`, and the environment variable `MOSA_HEAP_MAX` (also in MiB) sets it for `mosa` and for executables built by `mosac`. An unreadable `MOSA_HEAP_MAX` leaves the default; an unreadable `--heap-max` is a usage error. The limit also counts what the machine keeps outside the heap while it runs: the answers a running `findall/3`, `bagof/3` or `setof/3` has collected, the live choice points, the continuations and clause environments, and the goals that `call/1` translates (ADR 0010 amendments 2 and 3), so a recursion that leaves a choice point at every level stops at the limit too. When the heap reaches the limit, the query is stopped with `The heap reached its limit (N MiB); the query was stopped` on standard error: `mosa` goes on to the next query in the REPL, while a generated executable exits with status 2. The same stop happens before the limit is reached when, near the limit, three collections in a row each free less than 1/32 of the limit, since the program would otherwise slow to a crawl; raise the limit then. A program that stays within a small live set runs in a small heap: `loop/1` counting to 1000000 and `tak(18, 12, 6)` both finish with `--heap-max 1`.

`statistics(Key, Value)` has these keys: `heap` is `[Used, Capacity, Limit]` in bytes (`Capacity` is the soft capacity at which a collection normally starts; near the limit the trigger can lie above it); `garbage_collection` is `[Count, FreedBytes, Ms]`; `runtime` and `walltime` are `[TotalMs, MsSinceLastCall]`; `cputime` is CPU seconds as a float. `garbage_collect/0` forces a collection. The flag `trace_gc` (`set_prolog_flag(trace_gc, true)`) prints one line per collection to standard error. The environment variable `MOSA_GC_STRESS=N` collects at every N-th safe point; it is a testing aid.

When standard input is not a terminal, it belongs to Prolog's `user_input`, so a goal can read piped data:

```console
$ printf 'foo.\n' | ./build/mosa -g "read(X)." -t halt
X = foo.
```

### `mosac` (Prolog to C++20 translation and build)

`mosac` turns a Prolog file into a standalone executable. It needs one C++20 compiler (CMake is optional) and nothing else; the executable needs no compiler, no `mosac` and no Prolog files.

```sh
mosac build app.pl -o app          # translate, compile, link (app.exe on Windows)
mosac run app.pl -- arg...         # build in ./.mosac-cache, run a copy from a temp dir, return the exit code
mosac emit app.pl -o out/          # only the C++ source and CMakeLists.txt
mosac build app.pl --interactive   # the executable starts a REPL with app.pl loaded
mosac toolchain                    # show the compiler, CMake and bundled runtime, with remedies
```

Other options: `--debug` builds with `-O0 -g` and traces the program's own clauses (the bundled library units are not traced), `--verbose` shows the compiler commands, `--clean` deletes `.mosac-cache/` before building, `mosac run --keep` keeps the temporary directory that holds the copy of the executable, and `--runtime-source` builds the runtime from source instead of using the bundled static library. `mosac emit --library` and `mosac emit --units` exist for the bundled-library build and for `mosatest --diff`; you do not need them.

The compiler is taken from `--cxx`, then `MOSA_CXX`, then `CXX`, then the OS default (`clang++` on macOS, `g++` then `clang++` on Linux, MSVC found with `vswhere` on Windows). With CMake missing, with `MOSA_NO_CMAKE=1`, or on Windows with MSVC when the path of the runtime library or of `.mosac-cache/` has a character outside ASCII (CMake's NMake build can lose such characters), `mosac` calls the compiler directly, one generated file at a time. `MOSA_WERROR=1` compiles the generated C++ with warnings as errors (the tests and CI set it). With `:- initialization(main, main).` the executable runs `main` and exits like `mosa` does; `--interactive` skips it and starts the REPL. A call to a predicate that is defined nowhere gets a warning with its line that suggests `:- dynamic(Name/Arity).`, and the translation goes on; the executable raises `existence_error` when the goal runs, or follows the `unknown` flag, as `mosa` does. Intermediate files go to `.mosac-cache/`. See [`docs/SPEC.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.md) section 6.

### `mosatest` (test runner)

```sh
mosatest tests/pl/                    # Own tests (tests/pl/*.pl)
mosatest --suite tests/conformance    # Conformance suite; writes docs/conformance.md and .json
mosatest --suite <dir> --report tests/tmp/x.md   # Another suite; writes x.md and x.json
mosatest --roundtrip tests/pl/syntax.pl   # Read/write round trip
mosatest --diff tests/pl              # Interpreter against mosac output; writes docs/agreement.md and .json
mosatest --bench bench/nrev.pl --count 20000 --report tests/tmp/bench.json   # Interpreter and mosac timings, median of 5
mosatest --manual docs/manual         # The examples of the manual, in English and Japanese
```

`--report` may be omitted only when you pass the bundled `tests/conformance` (for `--diff`, also `tests/pl`) from the repository root. `--diff` also takes `--work`, `--cmake` and `--config`; see [`docs/bench.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.md) for `--bench`. This keeps another suite from overwriting the report and badge data in `docs/`.

## Japanese variables

Japanese has no upper and lower case, so `太郎` in `親(太郎, 一郎).` is an atom. A variable with a Japanese name starts with an underscore, as in `_太郎`. The full lexical rules are in section 4 of [`docs/SPEC.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.md), and [chapter 11 of the manual](docs/manual/en/11-japanese.md) shows them step by step.

```prolog
親(太郎, 一郎).
親(一郎, 花子).
祖父母(_祖, _孫) :- 親(_祖, _親), 親(_親, _孫).
```

(`親` = parent, `祖父母` = grandparent; `_祖` and `_孫` are variables.)

```console
$ ./build/mosa 家系.pl -g "祖父母(X, 花子)." -t halt
X = 太郎.
```

## Conformance

The pass rate on the bundled Logtalk conformance suite (`tests/conformance/logtalk/`) is shown by the badge above. At the end of phase 3, 2028 of 2171 tests (93.4%) passed; at the end of phase 4, 2041 (94.0%); at the end of phase 5, 2135 (98.3%); in 0.1.0, 2136 (98.4%). The list of failures is in [`docs/conformance.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.md). `mosatest --suite` writes that file and the badge data [`docs/conformance.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.json) (shields.io endpoint format). Do not edit either by hand.

The badge reads `docs/conformance.json` on `main`, so it changes with every commit that changes the file. CI runs the suite in all four jobs and puts the pass rate in the Job Summary; the macOS job fails if the generated JSON differs from the committed one. When a change alters suite results, run `mosatest --suite tests/conformance` and commit the JSON with it.

The "mosac diff" badge reads [`docs/agreement.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.json) on `main` in the same way. CI runs the differential tests on all three OSes (`ctest -L diff`, that is `mosatest --diff tests/pl` and `mosatest --diff tests/conformance`, with the generated C++ compiled with warnings as errors), puts the agreement in the Job Summary, and the macOS job fails if the generated JSON differs from the committed one. Each OS job also runs `mosac toolchain` and `mosac build tests/mosac/hello.pl`, and separate jobs download that executable and run it in a bare `ubuntu:24.04` container, check with `otool -L` on macOS that it depends only on `/usr/lib/` and `/System/`, and check with `dumpbin /dependents` on Windows that it depends only on OS DLLs. When a change alters the agreement, run `mosatest --diff tests/pl` and `mosatest --diff tests/conformance` and commit the JSON with it.

## Test layers

| Layer | Location | How to run |
| --- | --- | --- |
| C++ unit tests | `tests/cpp/` | `ctest` |
| Prolog tests | `tests/pl/` | `mosatest tests/pl/` (also run by `ctest`) |
| ISO conformance suite | `tests/conformance/` | `mosatest --suite tests/conformance` |
| Differential tests | `tests/pl/`, `tests/conformance/` | `mosatest --diff tests/pl` (also run by `ctest`, label `diff`) |
| Manual examples | `docs/manual/` | `mosatest --manual docs/manual` (also run by `ctest`) |

## Documents

Each document below has a Japanese translation next to it, as `*.ja.md` (for the manual, `docs/manual/ja/`). The English version is authoritative.

| Document | Contents |
| --- | --- |
| [`docs/manual/`](docs/manual/en/README.md) | The manual: installing Mosa, Prolog step by step, Japanese, modules, `mosac`, and a reference chapter |
| [`CHANGELOG.md`](CHANGELOG.md) | What each release has, its supported platforms, known limits and differences from ISO |
| [`docs/SPEC.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/SPEC.md) | The specification (goals, ISO scope, Japanese support, modules, architecture, tests, phases) |
| [`docs/STATUS.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/STATUS.md) | Current phase and next task (English only) |
| [`docs/adr/`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/adr/README.md) | Architecture decision records for choices that are hard to change later |
| [`docs/iso-notes.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/iso-notes.md) | Interpretation notes on the standard, by clause number |
| [`docs/conformance.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.md) | Conformance suite results (generated, Japanese headings) |
| [`docs/conformance.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/conformance.json) | Badge data for the pass rate (generated) |
| [`docs/agreement.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.md) | Interpreter / translated agreement measured by `mosatest --diff` (generated) |
| [`docs/agreement.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/agreement.json) | Badge data for the agreement (generated) |
| [`docs/unicode-version.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/unicode-version.md) | Unicode version used by the character tables |
| [`docs/bench.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.md) | Benchmark records (interpreter against translated code) |
| [`docs/bench.json`](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/bench.json) | Benchmark results the macOS CI job compares against (generated) |
| [`AGENTS.md`](https://github.com/SilentMalachite/mosa-prolog/blob/main/AGENTS.md) | Working rules for coding agents |

## Layout

```text
include/mosa/   Public headers
src/            core, read, write, machine, builtins, module, codegen, platform (compile is reserved and empty)
lib/            Standard library written in Prolog, and the message catalogs
bench/          Benchmark programs for mosatest --bench
tests/          cpp, pl, mosac, conformance, tmp
tools/          Generator scripts for developers (not needed to build)
docs/           Manual, spec, status, ADRs, interpretation notes
```

## License

Apache-2.0. The license text is in [`LICENSE`](LICENSE) and the copyright notice in [`NOTICE`](NOTICE). `tests/conformance/logtalk/` uses the same license; its Logtalk copyright notice is in `NOTICE.txt` in that directory.
