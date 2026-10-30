English | [日本語](README.ja.md)

# Mosa Prolog

[![CI](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml)
[![ISO conformance](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fconformance.json)](docs/conformance.md)
[![mosac diff](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fagreement.json)](docs/agreement.md)
[![Phase](https://img.shields.io/badge/phase-6_done-blue)](docs/STATUS.md)
[![Version](https://img.shields.io/badge/version-0.0.1-blue)](CMakeLists.txt)
[![ISO/IEC 13211-1](https://img.shields.io/badge/ISO%2FIEC-13211--1-informational)](docs/SPEC.md)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](CMakePresets.json)
[![Platforms](https://img.shields.io/badge/platform-macOS_%7C_Linux_%7C_Windows-lightgrey)](.github/workflows/ci.yml)
[![Unicode](https://img.shields.io/badge/Unicode-18.0.0-orange)](docs/unicode-version.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

Mosa Prolog is a Prolog system that follows ISO/IEC 13211-1. It is written in C++20 and depends only on the C++20 standard library and CMake. Japanese atoms and UTF-8 input/output are supported from the start.

## Name

Mosa stands for **M**odular **O**ptimized **S**ymbolic **A**rchitecture. Mosa Prolog is short for Modular Optimized Symbolic Architecture Prolog.

## Status

Phases 0–6 are done. Phase 7 is under way: the GC is done on the branch `phase7-gc`; the iterative writer and loader, the manuals and the release packages are next. Current progress is in [`docs/STATUS.md`](docs/STATUS.md).

| Phase | Scope | Status |
| --- | --- | --- |
| 0 | Skeleton (CMake, CI, test harness) | Done |
| 1 | Terms, reader, writer | Done |
| 2 | Execution core (IR, clause compiler, choice points, cut, `catch/throw`, REPL, `consult/1`) | Done |
| 3 | Built-in predicates, streams, flags, directives, error system | Done |
| 4 | Modules and `library(*)` | Done |
| 5 | Conformance push, full-width symbol mode, Japanese messages | Done |
| 6 | `mosac` (Prolog to C++20 translation and build), differential tests | Done |
| 7 | GC, manuals, release packages | In progress (GC done) |

The "mosac diff" badge is the agreement between the interpreter and the `mosac`-translated code that `mosatest --diff` measures over `tests/pl` and `tests/conformance` (`docs/agreement.md`, generated).

## Requirements

- CMake 3.25 or later, and Ninja (macOS and Linux)
- One of these compilers:
  - macOS: Apple Clang 15 or later
  - Linux: GCC 13 or later, or Clang 17 or later
  - Windows: Visual Studio 2022 (MSVC v17.8 or later)

## Build and test

Pick the preset for your OS: `macos-clang`, `linux-gcc`, `linux-clang`, or `windows-msvc`.

```sh
cmake --preset macos-clang
cmake --build --preset macos-clang
ctest --preset macos-clang
```

Executables go to `build/` (`build/Release/` on Windows).

## Usage

### `mosa` (interpreter)

```sh
mosa [file...] [-g goal] [-t halt] [--heap-max MiB] [-- argument...]
```

Started from a terminal with no goal, `mosa` enters interactive mode. A `-g` goal may end with `.` or leave it out, as in SWI-Prolog. A goal that cannot be read prints a syntax error; with `-t halt` the exit status is then 2.

Everything after `--` is passed to the program as the read-only flag `argv`, a list of atoms (`[]` without `--`). A file with `:- initialization(main, main).` calls `main` once after loading and then exits: 0 on success, 1 on failure, 2 after printing an uncaught exception, and the `halt/1` value if `main` halts. It then neither enters interactive mode nor runs `-g`.

```console
$ ./build/mosa tests/pl/nrev.pl -g "nrev([1,2,3], R)." -t halt
R = [3,2,1].
$ ./build/mosa -g "atom_length('𠮷野家', N)." -t halt
N = 3.
```

A character is always one Unicode scalar value, so `𠮷` counts as one character.

#### Heap, GC and `statistics/2`

The heap grows on demand up to a limit and is garbage-collected (mark and compact, ADR 0010); the default limit is 4 GiB. `--heap-max <MiB>` sets it for `mosa`, and the environment variable `MOSA_HEAP_MAX` (also in MiB) sets it for `mosa` and for executables built by `mosac`. An unreadable `MOSA_HEAP_MAX` leaves the default; an unreadable `--heap-max` is a usage error. When the heap reaches the limit, the query is stopped with `The heap reached its limit (N MiB); the query was stopped` on standard error: `mosa` goes on to the next query in the REPL, while a generated executable exits with status 2. The same stop happens before the limit is reached when, near the limit, three collections in a row each free less than 1/32 of the limit, since the program would otherwise slow to a crawl; raise the limit then. A program that stays within a small live set runs in a small heap: `loop/1` counting to 1000000 and `tak(18, 12, 6)` both finish with `--heap-max 1`.

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

The compiler is taken from `--cxx`, then `MOSA_CXX`, then `CXX`, then the OS default (`clang++` on macOS, `g++` then `clang++` on Linux, MSVC found with `vswhere` on Windows). With CMake missing, or with `MOSA_NO_CMAKE=1`, `mosac` calls the compiler directly, one generated file at a time. `MOSA_WERROR=1` compiles the generated C++ with warnings as errors (the tests and CI set it). With `:- initialization(main, main).` the executable runs `main` and exits like `mosa` does; `--interactive` skips it and starts the REPL. A call to a predicate that is defined nowhere makes `mosac emit` and `mosac build` stop and suggest `:- dynamic(Name/Arity).`. Intermediate files go to `.mosac-cache/`. See `docs/SPEC.md` section 6.

### `mosatest` (test runner)

```sh
mosatest tests/pl/                    # Own tests (tests/pl/*.pl)
mosatest --suite tests/conformance    # Conformance suite; writes docs/conformance.md and .json
mosatest --suite <dir> --report tests/tmp/x.md   # Another suite; writes x.md and x.json
mosatest --roundtrip tests/pl/syntax.pl   # Read/write round trip
mosatest --diff tests/pl              # Interpreter against mosac output; writes docs/agreement.md and .json
mosatest --bench bench/nrev.pl --count 20000 --report tests/tmp/bench.json   # Interpreter and mosac timings, median of 5
```

`--report` may be omitted only when you pass the bundled `tests/conformance` (for `--diff`, also `tests/pl`) from the repository root. `--diff` also takes `--work`, `--cmake` and `--config`; see [`docs/bench.md`](docs/bench.md) for `--bench`. This keeps another suite from overwriting the report and badge data in `docs/`.

## Japanese variables

Japanese has no upper and lower case, so `太郎` in `親(太郎, 一郎).` is an atom. A variable with a Japanese name starts with an underscore, as in `_太郎`. The full lexical rules are in section 4 of [`docs/SPEC.md`](docs/SPEC.md).

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

The pass rate on the bundled Logtalk conformance suite (`tests/conformance/logtalk/`) is shown by the badge above. At the end of phase 3, 2028 of 2171 tests (93.4%) passed; at the end of phase 4, 2041 (94.0%); at the end of phase 5, 2135 (98.3%). The list of failures is in [`docs/conformance.md`](docs/conformance.md). `mosatest --suite` writes that file and the badge data [`docs/conformance.json`](docs/conformance.json) (shields.io endpoint format). Do not edit either by hand.

The badge reads `docs/conformance.json` on `main`, so it changes with every commit that changes the file. CI runs the suite in all four jobs and puts the pass rate in the Job Summary; the macOS job fails if the generated JSON differs from the committed one. When a change alters suite results, run `mosatest --suite tests/conformance` and commit the JSON with it.

The "mosac diff" badge reads [`docs/agreement.json`](docs/agreement.json) on `main` in the same way. CI runs the differential tests on all three OSes (`ctest -L diff`, that is `mosatest --diff tests/pl` and `mosatest --diff tests/conformance`, with the generated C++ compiled with warnings as errors), puts the agreement in the Job Summary, and the macOS job fails if the generated JSON differs from the committed one. Each OS job also runs `mosac toolchain` and `mosac build tests/mosac/hello.pl`, and separate jobs download that executable and run it in a bare `ubuntu:24.04` container, check with `otool -L` on macOS that it depends only on `/usr/lib/` and `/System/`, and check with `dumpbin /dependents` on Windows that it depends only on OS DLLs. When a change alters the agreement, run `mosatest --diff tests/pl` and `mosatest --diff tests/conformance` and commit the JSON with it.

## Test layers

| Layer | Location | How to run |
| --- | --- | --- |
| C++ unit tests | `tests/cpp/` | `ctest` |
| Prolog tests | `tests/pl/` | `mosatest tests/pl/` (also run by `ctest`) |
| ISO conformance suite | `tests/conformance/` | `mosatest --suite tests/conformance` |
| Differential tests | `tests/pl/`, `tests/conformance/` | `mosatest --diff tests/pl` (also run by `ctest`, label `diff`) |

## Documents

Each document below has a Japanese translation next to it as `*.ja.md`. The English version is authoritative.

| Document | Contents |
| --- | --- |
| [`docs/SPEC.md`](docs/SPEC.md) | The specification (goals, ISO scope, Japanese support, modules, architecture, tests, phases) |
| [`docs/STATUS.md`](docs/STATUS.md) | Current phase and next task (English only) |
| [`docs/adr/`](docs/adr/README.md) | Architecture decision records for choices that are hard to change later |
| [`docs/iso-notes.md`](docs/iso-notes.md) | Interpretation notes on the standard, by clause number |
| [`docs/conformance.md`](docs/conformance.md) | Conformance suite results (generated, Japanese headings) |
| [`docs/conformance.json`](docs/conformance.json) | Badge data for the pass rate (generated) |
| [`docs/agreement.md`](docs/agreement.md) | Interpreter / translated agreement measured by `mosatest --diff` (generated) |
| [`docs/agreement.json`](docs/agreement.json) | Badge data for the agreement (generated) |
| [`docs/unicode-version.md`](docs/unicode-version.md) | Unicode version used by the character tables |
| [`docs/bench.md`](docs/bench.md) | Benchmark records (interpreter against translated code) |
| [`docs/bench.json`](docs/bench.json) | Benchmark results the macOS CI job compares against (generated) |
| [`AGENTS.md`](AGENTS.md) | Working rules for coding agents |

## Layout

```text
include/mosa/   Public headers
src/            core, read, write, machine, builtins, module, codegen, platform (compile is reserved and empty)
lib/            Standard library written in Prolog, and the message catalogs
bench/          Benchmark programs for mosatest --bench
tests/          cpp, pl, mosac, conformance, tmp
tools/          Generator scripts for developers (not needed to build)
docs/           Spec, status, ADRs, interpretation notes
```

## License

Apache-2.0. The license text is in [`LICENSE`](LICENSE) and the copyright notice in [`NOTICE`](NOTICE). `tests/conformance/logtalk/` uses the same license; its Logtalk copyright notice is in `NOTICE.txt` in that directory.
