English | [日本語](README.ja.md)

# Mosa Prolog

[![CI](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SilentMalachite/mosa-prolog/actions/workflows/ci.yml)
[![ISO conformance](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fconformance.json)](docs/conformance.md)
[![mosac diff](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FSilentMalachite%2Fmosa-prolog%2Fmain%2Fdocs%2Fagreement.json)](docs/agreement.md)
[![Phase](https://img.shields.io/badge/phase-3_done-blue)](docs/STATUS.md)
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

Phases 0–5 are done; phase 6 (`mosac`) is next. Current progress is in [`docs/STATUS.md`](docs/STATUS.md).

| Phase | Scope | Status |
| --- | --- | --- |
| 0 | Skeleton (CMake, CI, test harness) | Done |
| 1 | Terms, reader, writer | Done |
| 2 | Execution core (IR, clause compiler, choice points, cut, `catch/throw`, REPL, `consult/1`) | Done |
| 3 | Built-in predicates, streams, flags, directives, error system | Done |
| 4 | Modules and `library(*)` | Not started |
| 5 | Conformance push, full-width symbol mode, Japanese messages | Not started |
| 6 | `mosac` (Prolog to C++20 translation and build), differential tests | Not started |
| 7 | GC, manuals, release packages | Not started |

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
mosa [file...] [-g goal] [-t halt] [-- argument...]
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

When standard input is not a terminal, it belongs to Prolog's `user_input`, so a goal can read piped data:

```console
$ printf 'foo.\n' | ./build/mosa -g "read(X)." -t halt
X = foo.
```

### `mosatest` (test runner)

```sh
mosatest tests/pl/                    # Own tests (tests/pl/*.pl)
mosatest --suite tests/conformance    # Conformance suite; writes docs/conformance.md and .json
mosatest --suite <dir> --report tests/tmp/x.md   # Another suite; writes x.md and x.json
mosatest --roundtrip tests/pl/syntax.pl   # Read/write round trip
```

`--report` may be omitted only when you pass the bundled `tests/conformance` from the repository root. This keeps another suite from overwriting the report and badge data in `docs/`.

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
| [`docs/bench.md`](docs/bench.md) | Benchmark records |
| [`docs/bench.json`](docs/bench.json) | Benchmark results the macOS CI job compares against (generated) |
| [`AGENTS.md`](AGENTS.md) | Working rules for coding agents |

## Layout

```text
include/mosa/   Public headers
src/            core, read, write, machine, builtins, platform (module, compile, codegen come later)
lib/            Standard library written in Prolog (phase 4)
tests/          cpp, pl, conformance
tools/          Generator scripts for developers (not needed to build)
docs/           Spec, status, ADRs, interpretation notes
```

## License

Apache-2.0. The license text is in [`LICENSE`](LICENSE) and the copyright notice in [`NOTICE`](NOTICE). `tests/conformance/logtalk/` uses the same license; its Logtalk copyright notice is in `NOTICE.txt` in that directory.
