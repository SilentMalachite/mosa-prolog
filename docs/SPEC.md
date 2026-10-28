English | [日本語](SPEC.ja.md)

# Mosa Prolog Specification (SPEC)

This document reproduces sections 1–9 of the brief "ISO Prolog implementation (C++20 translation) — instructions for Claude Code and Grok Build" (指示書「ISO Prolog 処理系（C++20変換）— Claude Code Grok Build 向け指示書」). Sections 10 and 11 (agent working rules, repository layout) are in `AGENTS.md`.

## 1. Goals and non-goals

This document is the shared specification for implementing an ISO/IEC 13211-conformant Prolog system (named Mosa Prolog, repository name `mosa-prolog`) with Claude Code and Grok Build. Both CLAUDE.md and AGENTS.md must refer to this document (do not maintain the same content in two places).

Naming scheme: the commands are `mosa` (interpreter), `mosac` (translator), and `mosatest` (test runner). The runtime is `libmosa`, the headers are in `include/mosa/`, the C++ namespace is `mosa`, and the environment variable prefix is `MOSA_`. Always write lowercase `mosa`; use the formal name Mosa Prolog only in prose.

**Goals (in priority order)**

1. Conform to ISO/IEC 13211-1 (core) and achieve a high pass rate on publicly available conformance test suites.
2. Handle Japanese naturally in atoms, strings, identifiers, input/output, and source files (on a per-Unicode-code-point basis).
3. Provide a module system (based on the syntax of 13211-2, with practical SWI-Prolog-compatible semantics).
4. Translate Prolog programs into portable C++20 source, link them with the runtime, and produce standalone executables. Translation, compilation, linking, and execution are all done with a single mosac command; users do not operate the C++ toolchain directly.
5. The interactive interpreter (REPL, `consult/1`, the `assert` family) also runs on the same runtime. Translated programs and the interpreter must always produce the same results.

**Non-goals (not in scope this time)**

- Constraint logic programming (CLP(FD) etc.), tabling, parallel execution
- Arbitrary-precision integers (to be considered as an optional feature from Phase 6 onward; until then, 64-bit integers with `evaluation_error` on overflow)
- GUI, IDE integration, and tools other than a debugger
- Dependencies other than the C++20 standard library and CMake (no ICU, Boost, or GMP; GMP may become a future option)
- Dependencies on Windows-only or macOS-only APIs. OS-dependent code is isolated in `src/platform/` and kept within one file

## 2. Deliverables and supported environments

The deliverables are three executables and one library. All are built from a single CMake project.

| Deliverable | Contents | Form |
| --- | --- | --- |
| `libmosa` | Runtime containing term representation, unification, the solver, built-in predicates, streams, and module management | Static library (shared library optional) |
| `mosa` | Interactive interpreter. REPL, `consult/1`, command lines such as `-g Goal -t halt`, script execution | Executable |
| `mosac` | Prolog → C++20 translator. Goes all the way from a Prolog file to a standalone executable in one step (subcommands build / run / emit / dist / toolchain). C++ source output is an intermediate stage | Executable |
| `mosatest` | Test runner. Runs tests written in Prolog and the conformance suite, and reports results in JUnit format | Executable (Prolog script + thin wrapper) |

**Build environments (CI must pass on all three OSes)**

| OS | Compiler | Notes |
| --- | --- | --- |
| macOS (Apple Silicon) | Apple Clang 15 or later | Primary development environment |
| Linux (Ubuntu 24.04) | GCC 13 or later, Clang 17 or later | Reference CI environment |
| Windows 11 | MSVC 2022 (v17.8 or later) | `/utf-8` `/permissive-` `/Zc:__cplusplus` are required |

- The language standard is fixed at `-std=c++20` / `/std:c++20`. Do not use compiler extensions (GNU extensions, `__int128`, vendor-specific pragmas other than `#pragma once`).
- The only required tools are CMake 3.25 or later and the compilers above. Ninja is recommended.
- Generated C++ code follows the same constraints. Generated code includes only the `libmosa` header (`mosa/runtime.hpp`).
- Warnings are `-Wall -Wextra -Wpedantic` (MSVC: `/W4`), and CI treats warnings as errors.
- Character encoding is UTF-8 (without BOM) for all sources, tests, and documentation.

## 3. Scope of the language specification (ISO/IEC 13211-1, 13211-2)

The "core" is 13211-1:1995 plus Cor.1 (2007), Cor.2 (2012), and Cor.3 (2017); where they conflict, the Corrigenda take precedence. Where the standard and implementation decisions diverge, record it in `docs/iso-notes.md` with the clause number.

**Must be implemented in the core (in the chapter order of 13211-1)**

1. Syntax (chapter 6): tokenization, operator precedence, `op/3`, quoted atoms, escape sequences (including `\x...\`, `\uXXXX`, `\UXXXXXXXX`), `0'c` character code notation, handling of `{}`/`[]`, `end_of_file`
2. Term representation and unification (7.3), including `unify_with_occurs_check/2`. Default unification does not perform the occurs check
3. Execution model (7.7): search tree, cut, `catch/3`/`throw/1`, cut transparency of `call/1..8` (a cut inside `call/N` is local)
4. Error system (7.12): the `error(Formal, Context)` form and all Formal terms (`instantiation_error`, `uninstantiation_error/1`, `type_error/2`, `domain_error/2`, `existence_error/2`, `permission_error/3`, `representation_error/1`, `evaluation_error/1`, `resource_error/1`, `syntax_error/1`, `system_error`)
5. Arithmetic (chapter 9): integers are 64-bit signed, floats are IEEE 754 double precision. Provide the `max_integer`/`min_integer` flags; overflow raises `evaluation_error(int_overflow)`. Includes integer division, remainder, bitwise operations, trigonometric functions, `copysign`, and the functions added in Cor.2 such as `truncate`
6. All built-in predicates (chapter 8). The list and stages are given in section 7
7. Flags (7.11): `bounded`, `max_integer`, `min_integer`, `integer_rounding_function`, `char_conversion`, `debug`, `max_arity`, `unknown`, `double_quotes`. In addition, the implementation-defined flags `encoding` (always `utf8`), `version`, and `japanese_punctuation` (section 4)
8. Directives (7.4.2): `dynamic/1`, `discontiguous/1`, `multifile/1`, `op/3`, `char_conversion/2`, `initialization/1`, `set_prolog_flag/2`, `include/1`, `ensure_loaded/1`

**Decisions on implementation-defined matters**

| Matter | Decision |
| --- | --- |
| `max_arity` | 1024 |
| Default of `double_quotes` | `codes` (ISO default). Can be changed per module |
| Default of `unknown` | `error` |
| Character range | Unicode scalar values U+0000–U+10FFFF (surrogates are not allowed) |
| String type | None. `"..."` follows the `double_quotes` flag (SWI's string type is a non-goal) |
| Float format | Shortest round-trip representation (`std::to_chars`). Always contains `.`, exponent written with `e` |
| Integer syntax | `0b`, `0o`, `0x`, `0'c`, `_` digit separators (not specified in Cor.3, but allowed) |

**Intentional deviations from ISO (all are behind a flag or off by default, and are disabled when the `iso` flag is true)**

- Modules (section 5). 13211-2 has few implementations and known inconsistencies, so only its syntax is adopted; the semantics follow SWI-Prolog
- Acceptance of full-width symbols (section 4, off by default)
- Widely used non-ISO predicates such as `format/2,3`, `between/3`, `succ/2`, `msort/2`, `forall/2`, `nb_getval/2` are placed in modules such as `library(lists)` and `library(apply)`. They are not visible by default in the `user` module and are imported with `use_module`

## 4. Japanese and Unicode support

A "character" is always one Unicode scalar value, never a byte or a UTF-16 unit. This principle applies throughout all built-in predicates and the translator.

**Internal representation**

- Atom names are stored as UTF-8 (`std::string`), and the atom table deduplicates by byte sequence. No normalization (NFC/NFD) is performed, so `親` and a visually identical character written as a composed sequence are different atoms. The manual must state this explicitly
- `atom_length/2`, `sub_atom/5`, `atom_chars/2`, `char_code/2`, and `atom_codes/2` operate on code points. To keep the cost of `sub_atom/5` down, atoms carry an "ASCII only" flag, and for ASCII atoms byte positions are used directly
- Provide a fast path that creates a one-character atom directly from a `char32_t`

**Lexical analysis (the character classification of 13211-1 6.5 extended to Unicode)**

| ISO class | Unicode definition | Examples |
| --- | --- | --- |
| Lowercase (can start an alpha token) | `Ll`, `Lo`, `Lm`, `Nl`, and `Mn`/`Mc` (combining characters, cannot be first) | `親`, `たろう`, `カタカナ`, `ß` |
| Uppercase (starts a variable) | `Lu`, `Lt` | `X`, `Ä`, `Σ` |
| Digits | ASCII `0-9` only (full-width digits are not allowed) | `123` |
| Symbol characters | The 12 ISO characters + non-ASCII characters in `Sm`, `So`, `Pd`, `Po` | `+`, `→`, `≠`, `∧` |
| Layout (whitespace) | `Zs`, `Zl`, `Zp` + whitespace control characters | Includes the ideographic space U+3000 |

- Japanese has no letter case, so `太郎` in `親(太郎, 一郎).` is an **atom**. Variables with Japanese names start with an underscore, as in `_太郎`. This rule must be written in both the README and the REPL help
- The default mapping for `char_conversion/2` is the identity mapping. `char_conversion` does not perform full-width → half-width conversion or similar
- Single-kanji atoms can be written without quotes: `write(東)` outputs `東`. `writeq/1` decides whether quoting is needed according to the character classification. That is, `writeq(東京)` gives `東京` and `writeq('東 京')` gives `'東 京'`
- The Unicode classification tables do not rely on ICU; `tools/gen_unicode_tables.py` generates `src/unicode_tables.inc` from `UnicodeData.txt` (its version is pinned in `docs/unicode-version.md`). The generated file is committed to the repository, and the build does not require Python

**Full-width symbol mode (全角記号モード) (flag `japanese_punctuation`, default false)**

When true, the lexer treats the following full-width symbols as identical to their half-width counterparts. This is for source readability; `writeq/1` always outputs the half-width form.

| Full-width | Half-width |
| --- | --- |
| `（` `）` | `(` `)` |
| `［` `］` | `[` `]` |
| `，` `、` | `,` |
| `。` | `.` (clause terminator only; never used as a decimal point) |
| `：` | `:` |
| `「` `」` | `'` `'` (start and end of a quoted atom; cannot be nested) |

**Input/output**

- Every stream has an `encoding` option, defaulting to `utf8`. A `text` mode stream throws `representation_error(character)` on an invalid UTF-8 sequence, or replaces it with U+FFFD if the flag `invalid_utf8` is `replace`
- Standard input/output works in UTF-8 on Windows as well. `src/platform/windows.cpp` calls `SetConsoleCP(65001)` and `SetConsoleOutputCP(65001)`, and console input is received as UTF-16 via `ReadConsoleW` and converted to UTF-8. Files are opened through `std::filesystem::path` and must support Japanese path names
- `syntax_error` messages from `read_term/2,3` include the line number, the column number (in code points), and the offending token. Columns count one column per character, including full-width characters
- `~t~|` column alignment in `format/2,3` takes East Asian width into account (`East_Asian_Width` W/F count as width 2). This is also included in the generated tables

**Minimum set of Japanese tests (`tests/pl/japanese.pl`)**

- Read/write round trip of Japanese atoms and operators (after `:- op(700, xfx, は).`, `太郎 は 学生.` can be read)
- Extract `語処` from `日本語処理系` with `sub_atom/5`
- `atom_length('𠮷野家', 3)`. 𠮷 is the single code point U+20BB7 and is not counted as a UTF-16 surrogate pair
- `consult/1` of a file with a Japanese file name, and display of `syntax_error` messages containing Japanese
- Parse results are identical with full-width symbol mode on and off

## 5. Module system

The syntax adopts the common subset of ISO/IEC 13211-2 and SWI-Prolog, and the semantics follow SWI-Prolog's "predicate-based modules". One file corresponds to one module.

**Declarations and references**

```prolog
:- module(家系, [親/2, 祖父/2, (は)/2]).      % 第 1 節として置く。エクスポートは PI のリスト
:- use_module(library(lists)).                 % 全エクスポートを取り込む
:- use_module(人名, [姓名分解/3 as 分解]).     % 一部だけ取り込み、別名を付ける
:- use_module(人名, except([読み/2])).
:- meta_predicate 全員(1, ?).                  % 引数 1 はゴール（モジュール限定が付く）
:- export(新述語/1).                           % module/2 の後からの追加エクスポート

祖父(X, Z) :- 親(X, Y), 親(Y, Z).
?- 家系:祖父(太郎, Z).                         % 明示限定呼び出し
```

(The comments in the example say, in order: placed as the first clause, the exports are a list of PIs; imports all exports; imports only some and gives an alias; argument 1 is a goal (gets module-qualified); additional export after `module/2`; explicitly qualified call.)

**Resolution rules (searched in this order; the first definition found is used)**

1. The calling module's own definitions (local predicates and imported predicates share one namespace; a conflict raises `permission_error(import_into, ...)` at load time)
2. That module's default import source (normally `user`; the default for `user` is `system`)
3. The `system` module (all built-in predicates and control constructs; redefinition raises `permission_error(modify, static_procedure, PI)`)

- `Module:Goal` runs `Goal` in the context of `Module`. If `Module` is unknown, `existence_error(module, Module)`
- Predicates created by `dynamic/1`, `assertz/1`, etc. belong to the module that declared or added them. They can also be qualified, as in `assertz(家系:親(a, b))`
- Arguments specified with `:`, `0`–`9`, `^`, or `//` in `meta_predicate/1` are wrapped as `呼び出し元モジュール:引数` (calling module:argument) at call time. This makes `maplist/2,3`, `findall/3`, `catch/3`, `forall/2`, etc. work correctly across modules
- `multifile/1` takes the form `モジュール:PI` (Module:PI) and lets other modules add clauses
- The `user` module is the top-level default. `library(...)` is searched in `$MOSA_LIB` and in `lib/` relative to the executable
- `op/3` is local to the module that declares it. Writing `op(P, T, N)` in the export list makes it effective in importing modules too. Operators declared in `user` are visible in all modules
- The flags `double_quotes` and `japanese_punctuation` are saved and restored per module (more precisely, per file being loaded)

**Handling in the translator (`mosac`)**

- Each module corresponds to the C++ namespace `mosa::m::<mangled module name>`. Module names in UTF-8 cannot be used as C++ identifiers as-is, so they are mangled as `m_` + the name's bytes escaped as `_xx`. The original name is kept as a string constant and registered in the runtime module table
- Import resolution is done statically at translation time, and generated code uses direct function calls. Only calls through `call/N` and meta-predicates go through a runtime table lookup
- The translation unit is a "set of modules", and the transitive closure of `use_module` is included automatically. `library(...)` modules are linked as pre-translated objects

**Standard libraries to provide first (all written in Prolog; only those requiring a C++ implementation are placed in `system`)**

| Module | Main predicates |
| --- | --- |
| `library(lists)` | `append/3`, `member/2`, `memberchk/2`, `length/2` (ISO core, so placed in system and re-exported), `nth0/3`, `nth1/3`, `last/2`, `reverse/2`, `msort/2`, `sum_list/2`, `max_list/2`, `list_to_set/2` |
| `library(apply)` | `maplist/2..5`, `foldl/4..6`, `include/3`, `exclude/3` |
| `library(between)` | `between/3`, `succ/2`, `plus/3` |
| `library(format)` | `format/1,2,3` (`~w`, `~q`, `~a`, `~d`, `~f`, `~e`, `~s`, `~c`, `~n`, `~t`, `~\|`) |
| `library(strings)` | `atomic_list_concat/2,3`, `split_atom/4`, `upcase_atom/2` (Unicode uppercasing uses simple case mapping only) |
| `library(japanese)` | `ひらがな_カタカナ/2` (hiragana/katakana), `全角_半角/2` (full-width/half-width), `読み幅/2` (display width calculation). Implemented with the generated tables |

## 6. Architecture (runtime, interpreter, C++20 translation)

The interpreter and the translator share the same runtime (terms, heap, trail, built-in predicates); the only difference is "how clauses are executed". This lets differential tests guarantee that both produce the same results.

**Processing pipeline**

1. Lexical analysis (`lexer`): UTF-8 → token stream. Uses the character classification from section 4
2. Parsing (`reader`): operator precedence parser (Pratt method). Full support for the `read_term/3` options (`variable_names`, `singletons`, `quoted`, etc.)
3. Loading (`loader`): directive processing, module resolution, term expansion (`term_expansion/2` and `goal_expansion/2` are looked up in `user` and in the module being loaded)
4. Clause compilation (`clause_compiler`): lowers clauses to the IR (intermediate representation). The IR is the common input to the interpreter and `mosac`
5. Execution: the interpreter interprets the IR directly. `mosac` generates C++20 from the IR

**Core of the runtime (`libmosa`)**

| Component | Design |
| --- | --- |
| Cell representation | 64-bit tagged integer `Cell`. The low 3 bits are the tag: REF, ATOM, INT (61-bit immediate), STR (pointer to a functor cell in the heap), LIS (list), FLT (boxed double), BIG (reserved for the future). Integers that do not fit in 61 bits are boxed like FLT, and `max_integer` stays 2^63−1 |
| Heap | A single contiguous array (allocated as `std::vector<Cell>`, with an upper bound reserved to avoid reallocation). Reclaimed on backtracking simply by resetting the top |
| Trail | Pushes the addresses of bound REFs. Conditional trailing (records only cells older than the choice point) |
| Choice point stack | Holds the continuation, trail position, heap position, pointer to the alternative clause, and cut barrier |
| GC | None until Phase 5 (only reclamation by backtracking). A mark-and-compact collector is added in Phase 7. Provide `garbage_collect/0` |
| Atom table | UTF-8 byte sequence → 32-bit atom number. Does not need to be thread-safe (non-goal) |
| Exceptions | Prolog's `throw/1` does not use C++ exceptions; it pushes `catch` frames on the choice point stack and unwinds them. C++ exceptions are used only for fatal errors inside the runtime (`resource_error`) |
| Streams | `Stream` abstract class + implementations for files / standard I/O / memory (equivalent to `open_string`). Buffering and peeking (`peek_char/1`) are built in |

**Execution model: binarization + explicit continuations**

A clause `h :- b1, b2, b3.` is transformed (binarized) into a form with the continuation as the last argument, `h(K) :- b1(b2(b3(K)))`. This removes the need for the WAM environment stack and makes translation to C++ simple functions and loops.

- Each predicate corresponds to a C++ function of the form `bool pred_X_N(Machine& m, Cell* args, Cont k)`. `Cont` is a small struct representing "the next predicate and its arguments" and is placed on the heap
- The execution loop is a trampoline: `while (goal = m.pop()) { dispatch(goal); }`. C++ recursion is not used, so deep recursion does not overflow the stack
- Choice points are created when switching between clauses inside a predicate body. The clause compiler performs first-argument indexing (type tag + functor), and no choice point is created when there is a single candidate
- Cut passes the height of the choice point stack as the cut barrier in an argument. `call/N` uses the height at call time as the new barrier, which makes cut local

**C++ generated by `mosac`**

- One `.cpp` per module, plus a header declaring the exported predicates. Predicate names are `p_` + the mangled name + `_` + arity
- Generated code includes only `mosa/runtime.hpp` and uses nothing beyond the C++20 standard library
- Clause bodies are state machines using `switch` and `goto`, with one state per goal in the clause. No macros; use `inline` functions and `constexpr` (to keep generated code readable and traceable in a debugger)
- Only static predicates are translated. `dynamic` predicates and clauses added by `assert` run on the interpreter path (IR interpretation) and coexist on the same `Machine`
- The output includes `main.cpp` (a call equivalent to `initialization(main)`) and a `CMakeLists.txt` fragment, so that `mosac -o out/ app.pl && cmake -S out -B out/build && cmake --build out/build` produces an executable
- So that a human can read generated code and trace it back to the original Prolog, the original clauses are inserted as comments at the start of each predicate (they may stay in Japanese)

**`mosac` build and run pipeline (do not stop at emitting C++)**

The default behavior of `mosac` is "take a Prolog file and produce a standalone executable". C++ source is only an intermediate artifact; users never touch CMake or the compiler directly.

| Subcommand | Behavior | Output |
| --- | --- | --- |
| `mosac build app.pl -o app` | Translate → compile → link in one step. The default | Executable `app` (`app.exe` on Windows) |
| `mosac run app.pl -- 引数...` | build, then run immediately. Built in a temporary directory; the exit code is returned as-is | Execution result |
| `mosac emit app.pl -o out/` | Outputs only C++ source and `CMakeLists.txt` (for people who want to modify it by hand, and for debugging) | `out/` |
| `mosac dist app.pl -o dist/` | build + distribution package (executable, LICENSE, README) | zip / tar.gz |
| `mosac toolchain` | Shows the location and version of the detected compiler, CMake, and bundled runtime. If there is a problem, prints remedies in Japanese | Diagnostic output |

(`引数...` in the `run` row means "arguments...".)

**Toolchain handling**

- The C++ compiler is searched for in this order: `--cxx` → environment variable `MOSA_CXX` → `CXX` → OS default (`clang++` on macOS; `g++` then `clang++` on Linux; on Windows, find MSVC with `vswhere`, and `mosac` itself sets the environment variables equivalent to `vcvarsall`). If none is found, print the same diagnostics as `mosac toolchain` and stop, showing the installation steps for each OS (Xcode Command Line Tools, `apt install g++`, Visual Studio Build Tools)
- CMake is optional. Use it if present; otherwise invoke the compiler directly (compile the generated code file by file, and `mosac` itself constructs the link command). The minimum requirement on the user side is therefore "one C++20 compiler is installed"
- The runtime `libmosa` is shipped as a prebuilt static library and headers in the `mosac` installation directory (`<prefix>/lib/mosa/<OS>-<arch>-<compiler>/`). The runtime is not rebuilt on the user's machine every time. As a fallback when the ABI does not match, `--runtime-source` builds it from the bundled source (the result is cached)
- The standard libraries in `lib/*.pl` are shipped as pre-translated objects, and only those loaded with `use_module` are linked
- The default is static linking with `-O2`. On Windows, `/MT`. The resulting executable needs neither a C++ runtime DLL, nor `mosac`, nor any Prolog files on the target machine. `--debug` enables `-O0 -g` and runtime clause tracing
- Intermediate files are placed in `.mosac-cache/` and reused based on hashes of the Prolog source and the runtime. `--keep` keeps the generated C++, and `--clean` discards it
- Cross-compilation is a non-goal. Executables are built only for the OS and architecture on which `mosac` runs

**Error handling (reported differently depending on whose responsibility it is)**

- Problems in the user's Prolog (syntax errors, export inconsistencies, static warnings for undefined predicates) → stop at the translation stage and report with Prolog line numbers. The compiler is not started
- Generated C++ fails to compile → treated as a `mosac` bug. Print only "内部エラー: 生成コードがコンパイルできません。`.mosac-cache/<hash>/` を添えて報告してください" (Internal error: the generated code cannot be compiled. Please report it with `.mosac-cache/<hash>/` attached), and show the raw compiler errors only with `--verbose`. In CI, "generated C++ always compiles" is an invariant, checked across all differential test cases
- Compiler not found, insufficient disk space, permissions → report as environment problems, with remedies

**Behavior of the resulting executable**

- If `:- initialization(main, main).` is present, it is the entry point. If `main/0` succeeds, the exit code is 0; if it fails, 1; on an uncaught exception, print a message and exit with 2. The argument of `halt/1` becomes the exit code as-is
- Command-line arguments are received through the flag `argv` (a list of atoms)
- An executable built with `mosac build --interactive` starts a REPL with the translated code already loaded (translated code and the interpreter coexist as described above)
- The executable requires no external files at startup. All needed Prolog libraries are already linked, and it does not search for `lib/`

**Source layout**

| Directory | Contents |
| --- | --- |
| `src/core/` | Cell, heap, trail, atoms, unification |
| `src/read/` | lexer, reader, operator table, Unicode tables |
| `src/write/` | `write_term/3`, `writeq`, float formatting, quoting decisions |
| `src/machine/` | Execution loop, choice points, catch/throw, IR definitions |
| `src/builtins/` | Built-in predicates (one file per ISO chapter: `control.cpp`, `arith.cpp`, `atoms.cpp`, `streams.cpp`, ...) |
| `src/module/` | Module table, import resolution, meta\_predicate |
| `src/compile/` | Clause compiler (IR generation, indexing) |
| `src/codegen/` | IR → C++20 generation |
| `src/platform/` | OS-dependent code (Windows console, path conversion). Do not write `#ifdef _WIN32` anywhere else |
| `lib/` | Standard libraries written in Prolog |

## 7. Built-in predicates and the error system

13211-1 chapter 8 has about 100 built-in predicates in total. The implementation order follows dependencies, proceeding from the top of the table below. For each predicate, write "the clause number in the standard, the argument modes, the errors it may throw, and the test file name" in the comment at the top of the file in `src/builtins/`.

| Stage | Standard clause | Predicates |
| --- | --- | --- |
| A (first) | 8.2 term unification, 8.3 type testing, 8.4 term comparison | `=/2`, `\=/2`, `unify_with_occurs_check/2`, `var/1`, `atom/1`, `integer/1`, `float/1`, `atomic/1`, `compound/1`, `nonvar/1`, `number/1`, `callable/1`, `is_list/1` (non-ISO, system), the 6 predicates such as `@</2`, `compare/3`, `==/2`, `\==/2` |
| A | 8.5 term creation and decomposition | `functor/3`, `arg/3`, `=../2`, `copy_term/2`, `term_variables/2` |
| A | 7.8 control constructs, 8.15 logic and control | `true/0`, `fail/0`, `false/0`, `!/0`, `,/2`, `;/2`, `->/2`, `*->/2` (non-ISO), `if/3` (non-ISO, `(C *-> T ; E)` with a local cut), `catch/3`, `throw/1`, `call/1..8`, `\+/1`, `once/1`, `repeat/0`, `halt/0,1` |
| B | 9 arithmetic, 8.6, 8.7 | `is/2`, the 6 predicates such as `=:=/2`, all evaluable functors (`+ - * / // mod rem div abs sign min max gcd ** ^ sqrt sin cos tan asin acos atan atan2 exp log/1 log/2 log2 float integer float_integer_part float_fractional_part truncate round ceiling floor >> << /\ \/ xor \ pi e inf nan epsilon max_tagged_integer msb copysign nexttoward succ_or_zero`) |
| B | 8.16 atomic term processing | `atom_length/2`, `atom_concat/3`, `sub_atom/5`, `atom_chars/2`, `atom_codes/2`, `char_code/2`, `number_chars/2`, `number_codes/2`, `atom_number/2` (non-ISO) |
| C | 8.8, 8.9 clause retrieval and creation/destruction | `clause/2`, `current_predicate/1`, `asserta/1`, `assertz/1`, `retract/1`, `abolish/1`, `retractall/1` (Cor.2) |
| C | 8.10 all solutions | `findall/3`, `findall/4`, `bagof/3`, `setof/3` (handling of `^/2`, sorting in the standard order of terms) |
| D | 8.11 stream selection and control | `current_input/1`, `current_output/1`, `set_input/1`, `set_output/1`, `open/3,4`, `close/1,2`, `flush_output/0,1`, `stream_property/2`, `at_end_of_stream/0,1`, `set_stream_position/2` |
| D | 8.12 character I/O, 8.13 byte I/O | `get_char/1,2`, `get_code/1,2`, `peek_char/1,2`, `peek_code/1,2`, `put_char/1,2`, `put_code/1,2`, `nl/0,1`, `get_byte/1,2`, `peek_byte/1,2`, `put_byte/1,2` |
| D | 8.14 term I/O | `read_term/2,3`, `read/1,2`, `write_term/2,3`, `write/1,2`, `writeq/1,2`, `print/1,2`, `write_canonical/1,2`, `op/3`, `current_op/3`, `char_conversion/2`, `current_char_conversion/2` |
| E | 8.17 implementation-defined hooks | `set_prolog_flag/2`, `current_prolog_flag/2`, `halt/0,1` (done earlier in A) |
| E | 7.4.2 directives, loading | `consult/1`, `[File]`, `ensure_loaded/1`, `include/1`, `initialization/1`, `dynamic/1`, `discontiguous/1`, `multifile/1` |
| F | Section 5 | Module-related (`module/2`, `use_module/1,2`, `current_module/1`, `predicate_property/2`, `meta_predicate/1`, `export/1`) |
| G | Non-ISO utilities (`library(*)`, the table in section 5) | `format/1,2,3`, `between/3`, `succ/2`, `msort/2`, `sort/4`, `sort/2` and `keysort/2` (ISO core, so in system), `forall/2`, `nb_setval/2`, `nb_getval/2`, `tab/1,2`, `term_to_atom/2`, `number_vars/3`, `setup_call_cleanup/3`, `garbage_collect/0`, `statistics/2` |

**Rules of the error system**

- All errors are thrown in the form `throw(error(Formal, Context))`. `Context` is `context(PI, Extra)`, where `PI` is the predicate indicator and `Extra` is additional information (for `syntax_error`, the form `stream(S, Line, LinePos, CharNo)` rather than `位置(Line, Col)`). ISO permits implementation-defined terms in `Context`
- The order of argument checks strictly follows each predicate's definition in ISO (the order listed in 8.x.y.3 Errors). For example, `atom_length(X, foo)` does not throw `type_error(integer, foo)`; it checks `X` first and throws `instantiation_error`. Order violations are detected by the conformance suite
- On the C++ side, use only the single family of helpers `throw_error(m, Formal, PI)`, and build each kind of `Formal` with functions such as `make_type_error(m, Type, Culprit)`. Writing string literals directly is forbidden (to prevent typos in atom names)
- Arithmetic errors: division by zero is `evaluation_error(zero_divisor)`, integer overflow is `evaluation_error(int_overflow)`, float overflow is `evaluation_error(float_overflow)`, and `sqrt(-1)` etc. is `evaluation_error(undefined)`. `inf`/`nan` are thrown when the `float_overflow`/`undefined` flag is `error`; otherwise IEEE 754 values are returned (SWI-compatible `float_overflow`, `float_zero_div`, `float_undefined` flags)
- The top level displays uncaught exceptions in the form `警告: ゴールは例外を送出しました: error(...)` (Warning: goal raised exception: error(...)) in Japanese or English (flag `message_language`, whose default is determined from the environment variable `LANG`). Message strings are placed as Prolog facts in `lib/messages_ja.pl` and `lib/messages_en.pl`, and are not embedded in C++

## 8. Test strategy

Tests have four layers. Two numbers, "conformance suite pass rate" and "interpreter/translator agreement rate", are updated as README badges on every commit.

| Layer | Contents | How to run |
| --- | --- | --- |
| 1. C++ unit tests | Components that exist before Prolog starts: Cell, atoms, lexer, unification, UTF-8 conversion, etc. | No external framework; a minimal `assert`-based runner in `tests/cpp/` (`tests/cpp/harness.hpp`, 50 lines or fewer). Registered with CTest |
| 2. Own Prolog tests | `tests/pl/<chapter>.pl`. Written as facts `test(名前, ゴール, 期待)` (name, goal, expected), loaded and run by `mosatest`. The expectation is one of 4 kinds: `true`, `fail`, `error(Formal)`, `solutions([X=1, X=2])` | `mosatest tests/pl/` |
| 3. ISO conformance suite | Logtalk's Prolog conformance tests are placed unmodified in `tests/conformance/logtalk/`. They are Apache-2.0, the same as the repository, and the copyright notices are kept in `NOTICE.txt` there. The INRIA Suite (`inriasuite.pl`) is not bundled because there is no permission to redistribute it. Ulrich Neumerkel's tables are not copied. Run with `mosatest --suite`, writing the pass count / total and the list of failures to `docs/conformance.md` | Every CI run. Changes that lower the pass rate are not merged |
| 4. Differential tests | All goals from layers 2 and 3 are translated, built, and run with `mosac` run, and compared byte for byte with the interpreter's output (solutions, error terms, standard output) | `mosatest --diff`. Required in CI from Phase 6 onward |

**Rules for writing tests**

- When implementing a new built-in predicate, first copy the standard's Examples (8.x.y.4) verbatim into `tests/pl/`, then implement. Each line of the Examples is one test
- Error tests compare only the `Formal` term, not `Context` (because it is implementation-defined)
- Japanese tests (section 4) are placed in layer 2 as `tests/pl/japanese.pl` and must always be included in the layer 4 differential tests
- Input/output tests use memory streams (`open_string`, and a `system` predicate equivalent to `with_output_to/2`), and temporary files are created only under `tests/tmp/`
- On Windows CI, standard output is captured as UTF-8 for comparison (put `chcp 65001` in the workflow)

**Benchmarks (lower priority than pass rate, but used to detect regressions)**

- `bench/`: `nrev` (naive reverse, LIPS measurement), `queens`, `zebra`, `tak`, `deriv`, `crypt`. Measured with both the interpreter and the `mosac` translation, and recorded as a table in `docs/bench.md`
- No target values are set. CI warns on a regression of 20% or more compared with the previous run

## 9. Development phases and milestones

Move to the next phase only when the current phase's exit criteria are met. Exit criteria are limited to things that can be checked mechanically (tests pass, a command works). One session handles only one task of one phase.

| Phase | Contents | Exit criteria |
| --- | --- | --- |
| 0 | Skeleton: CMake, directories, CI (3 OSes), `harness.hpp`, `docs/STATUS.md`, `docs/adr/0001-cell-representation.md` | An empty test passes on 3 OSes. `mosa --version` works |
| 1 | Terms and reader/writer: Cell, heap, atoms, unification, Unicode table generation, lexer, reader (including `op/3`), `write_term/3` | `tests/pl/syntax.pl` (examples from chapter 6 of the standard) passes as a read/write round trip. Round trip of Japanese atoms passes |
| 2 | Execution core: IR, clause compiler (first-argument indexing), trampoline execution loop, choice points, cut, `catch/throw`, stage A built-in predicates, minimal REPL, `consult/1` | `nrev` and `queens` run. All standard Examples for stage A pass |
| 3 | Expanding built-in predicates: stages B–E, streams in general, flags, directives, completing the error system | Pass rate of 90% or more on the bundled Logtalk conformance suite. `docs/conformance.md` is generated automatically |
| 4 | Modules: all of section 5, the first 6 modules of `library(*)`, `meta_predicate` | `tests/pl/modules.pl` passes. Existing tests that use `library(lists)` do not break |
| 5 | Raising the conformance rate and polishing Japanese support: fixing failing items, full-width symbol mode, Windows console, Japanese messages | Raise the pass rate on the bundled Logtalk conformance suite. All of `tests/pl/japanese.pl` passes |
| 6 | `mosac`: IR → C++20 generation, build pipeline (build / run / emit / toolchain, toolchain detection, bundled runtime), differential tests | Zero differences across all layer 2 and 3 tests. On 3 OSes, the executable built by mosac build hello.pl runs in a clean environment with no compiler, CMake, or mosac installed. The translated `nrev` is at least 3 times faster than the interpreter |
| 7 | Finishing: GC, `garbage_collect/0`, `statistics/2`, manual (Japanese and English), mosac dist, release packages (binaries for each OS, bundled runtime) | Memory usage plateaus in long-running loops. A newcomer can build using only the steps in the README |

**Prohibitions across phases**

- Do not change the Cell representation decided in Phase 1 or the IR decided in Phase 2 until an ADR (`docs/adr/`) has been written and approved
- Do not add features from later phases "while you are at it". Do not write groundwork for GC or arbitrary-precision integers before Phase 7 either
- Do not leave conformance suite failures unaddressed as "differences in interpreting the spec" without recording them in `docs/iso-notes.md`
