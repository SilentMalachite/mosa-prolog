English | [日本語](AGENTS.ja.md)

# AGENTS.md — Mosa Prolog

This file is the body of the agent instructions shared by Claude Code and Grok Build. Claude Code reads it via `CLAUDE.md` (whose content is the single line `@AGENTS.md`), and Grok Build reads `AGENTS.md` directly. There are no per-agent differences. `AGENTS.ja.md` is a Japanese translation for humans and is not loaded by agents.

## Project overview

- Implement **Mosa Prolog** (repository `mosa-prolog`), a Prolog system conforming to ISO/IEC 13211, in C++20
- Commands: `mosa` (interpreter), `mosac` (Prolog → C++20 translation and build), `mosatest` (test runner). The runtime is `libmosa`, headers are in `include/mosa/`, the namespace is `mosa`, and the environment variable prefix is `MOSA_`
- The only dependencies are the C++20 standard library and CMake 3.25 or later. CI must pass on 3 OSes: macOS (Apple Clang 15+), Linux (GCC 13+ / Clang 17+), Windows (MSVC 2022 v17.8+)
- All text encoding is UTF-8 (no BOM). A "character" is always exactly one Unicode scalar value

## Where the specification lives

The body of the specification is in `docs/SPEC.md` (do not duplicate it in this file). Read the relevant section before starting work.

| What you want to know | Section of `docs/SPEC.md` |
| --- | --- |
| Goals and non-goals | Section 1 |
| Deliverables, build environment, compiler constraints | Section 2 |
| ISO coverage, implementation-defined items, intentional deviations | Section 3 |
| Japanese and Unicode (lexical classification, full-width symbol mode, I/O) | Section 4 |
| Module system | Section 5 |
| Architecture (Cell, execution model, `mosac`, source layout) | Section 6 |
| Built-in predicate implementation order and error system | Section 7 |
| Test strategy | Section 8 |
| Phases and exit criteria | Section 9 |

## Working rules for agents

### Starting a session (always, every time)

1. Read `docs/STATUS.md`. It states "the current phase", "the task in progress" and "unresolved items from last time" in 15 lines or fewer
2. Check `git log --oneline -10` and the result of `ctest`. If any test is still broken, fix it before starting new work
3. Pick one task for this session and write it under "In progress" in `STATUS.md` before starting. A task is one of: "one predicate", "one file", or "one bug"

### Ending a session (always, every time)

1. Make `cmake --build build && ctest --test-dir build` pass. If you end in a state where it does not pass, write that fact and the reproduction steps in `STATUS.md`
2. Update `STATUS.md`: what was completed, the one next thing to do, and the points you were unsure about. If it exceeds 15 lines, delete older entries
3. Commit changes one logical unit at a time. The message is one line in Japanese giving the phase, the content and how it was verified, like `[phase2] functor/3 を実装（8.5.1 の Examples 全通過）` ("implement functor/3 (all Examples in 8.5.1 pass)")

### Handling design decisions

- Decisions that are hard to change later, such as the Cell representation, the IR, module resolution and the format of error `Context`, must be written in `docs/adr/NNNN-title.md` as "Context, Decision, Rejected alternatives, Consequences", each in 3 lines or fewer. Do not implement before the ADR is written
- If you notice that the specification (`docs/SPEC.md` sections 3–7) disagrees with the standard, do not fix it on the spot. Write it in `docs/iso-notes.md` with the clause number and ask the project owner to confirm. The confirmation must be a single question of the form "A or B?"
- When you write a "just make it work for now" implementation at a point where you are unsure, attach the standard's clause number in the code, like `// TODO(iso): 8.16.3.3 の順序未確認` ("ordering in 8.16.3.3 not yet confirmed"). Do not write a TODO without a clause number

### Writing code

- Identifiers are in English. Code comments and commit messages are in Japanese. Documentation is English-primary: the English file has the canonical name (`README.md`, `docs/SPEC.md`, …) and its Japanese translation sits next to it as `*.ja.md`; when you change one, update the other in the same commit. If they disagree, the English version wins. One-line directory READMEs carry the English sentence first and the Japanese sentence below it in the same file. `docs/STATUS.md` is English only, and `docs/conformance.md` is generated and stays as the tool writes it. In comments, explain why this order or this method, not what the code does
- A function must be 60 lines or fewer, and a file 800 lines or fewer. If it would exceed that, split first
- Do not use raw `new`/`delete`, `reinterpret_cast`, C arrays or macros. Use `std::span`, `std::string_view`, and the equivalent of `std::expected` (`std::optional` + an error type). Exceptions follow the rules in `docs/SPEC.md` section 6 (do not use C++ exceptions for Prolog's `throw/1`)
- Write the C++ function of a built-in predicate in the order argument checks → main processing → binding. Do not write it so that it binds partway and then fails
- The header comment of a built-in predicate's file must give "the standard's clause number, the argument modes, the errors it can throw, and the test file name"
- Throw errors only through the `throw_error(m, Formal, PI)` and `make_type_error(m, Type, Culprit)` family of helpers. Writing atom names as string literals is forbidden
- Do not write OS-dependent code such as `#ifdef _WIN32` anywhere except `src/platform/`
- Do not use compiler extensions (GNU extensions, `__int128`, vendor pragmas other than `#pragma once`). Warnings are treated as errors with `-Wall -Wextra -Wpedantic` (`/W4` for MSVC)
- Do not add new dependency libraries. If you think one is needed, write the reason in `STATUS.md` and stop

### Writing tests

- For a new built-in predicate, copy the standard's Examples (8.x.y.4) verbatim into `tests/pl/` before implementing it. Each line of the Examples is one test
- Error tests compare only the `Formal` term and do not compare `Context`
- I/O tests use memory streams, and temporary files are created only under `tests/tmp/`
- Japanese tests go in `tests/pl/japanese.pl` and must always be included in the differential test (`mosatest --diff`)

### Prohibitions across phases

- Do not change the Cell representation decided in Phase 1 or the IR decided in Phase 2 until an ADR is written and approved
- Do not add features of later phases "while you are at it". Do not write groundwork for GC or bignums before Phase 7 either
- Do not leave a conformance suite failure unattended without recording it in `docs/iso-notes.md` as "a difference in interpreting the specification"

### Things you must not do

- Weaken tests, skip them, or rewrite expected values to match the implementation in order to make tests pass
- Lean toward SWI on the grounds that "ISO says this but SWI differs". When the `iso` flag is true, the behavior is always ISO
- Touch multiple phases in one session. Move on to the next phase without meeting the phase's exit criteria
- End without updating `STATUS.md`. The standard is that your next-session self (or another agent) can resume without the conversation log
- Write a long summary in a report to the project owner. A report needs only "one line on what was done, one line on what is next, one line with a question (if any)"

## Repository layout and file placement

Create the following exactly as shown in Phase 0. In an empty directory, put a `README.md` with one line saying what goes there, not a `.gitkeep`.

```text
mosa-prolog/
├── AGENTS.md                 # This file. The body read by Grok Build and Claude Code
├── AGENTS.ja.md              # Japanese translation for humans. Agents do not load it
├── CLAUDE.md                 # 1 line: @AGENTS.md
├── README.md                 # Build steps, rules for Japanese variable names, conformance badge
├── README.ja.md              # Japanese translation of README.md
├── LICENSE                   # Apache-2.0
├── CMakeLists.txt
├── CMakePresets.json         # macos-clang / linux-gcc / linux-clang / windows-msvc
├── .github/workflows/ci.yml  # 3-OS matrix, warnings as errors, conformance rate in the Job Summary
├── docs/
│   ├── SPEC.md               # Specification (sections 1–9 of the instructions document)
│   ├── SPEC.ja.md            # Japanese translation of SPEC.md
│   ├── STATUS.md             # Current position. 15 lines or fewer. English only
│   ├── adr/                  # Numbered sequentially from 0001-cell-representation.md
│   ├── iso-notes.md          # Notes on interpreting the standard (with clause numbers)
│   ├── iso-notes.ja.md       # Japanese translation of iso-notes.md
│   ├── conformance.md        # Generated automatically. Do not edit by hand
│   ├── bench.md
│   ├── unicode-version.md
│   └── manual/               # ja/ and en/. Phase 7
├── include/mosa/
│   ├── runtime.hpp           # The only header that generated code includes
│   └── mosa.hpp              # C++ API for embedding (creating a Machine, consult, query)
├── src/
│   ├── core/  read/  write/  machine/  builtins/  module/  compile/  codegen/  platform/
│   ├── unicode_tables.inc    # Generated. Committed
│   ├── main_mosa.cpp
│   ├── main_mosac.cpp
│   └── main_mosatest.cpp
├── lib/                      # Standard library written in Prolog (lists.pl, apply.pl, format.pl, japanese.pl, messages_ja.pl, messages_en.pl …)
├── tests/
│   ├── cpp/                  # harness.hpp and component tests
│   ├── pl/                   # Our own tests, per chapter. Includes japanese.pl and modules.pl
│   ├── conformance/          # External suites (license files bundled)
│   └── tmp/                  # .gitignore
├── bench/
└── tools/
    └── gen_unicode_tables.py # Run by developers only. Not needed for the build
```

### Initial contents of each file

- Initial content of `docs/STATUS.md`: `フェーズ: 0 / 進行中: なし / 次: CMake と harness.hpp / 未解決: なし` ("Phase: 0 / In progress: none / Next: CMake and harness.hpp / Unresolved: none")
- `docs/adr/0001-cell-representation.md`: copy the Cell design in `docs/SPEC.md` section 6 into ADR form. This is the first ADR, and the project owner approves it before Phase 1 starts
- `CMakePresets.json` must have `configurePresets`, `buildPresets` and `testPresets`, so that everything works with the 3 commands `cmake --preset macos-clang && cmake --build --preset macos-clang && ctest --preset macos-clang`
- Keep the API of `include/mosa/mosa.hpp` minimal: `Machine::create()`, `consult(path)`, `query(text, callback)`, `halt()`. Add nothing else before Phase 7

### First tasks of Phase 0

1. Create the tree above and put `AGENTS.md`, `CLAUDE.md`, `docs/SPEC.md` and `docs/STATUS.md` in place
2. Write `CMakeLists.txt` and `CMakePresets.json`, and build an executable that does nothing but print `mosa 0.0.1 (phase 0)` for `mosa --version`
3. Register `tests/cpp/harness.hpp` and one test that only checks `1 + 1 == 2`, and make it pass with `ctest`
4. Write `.github/workflows/ci.yml`, confirm it is green on 3 OSes, then update `STATUS.md` and finish
