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
- GMP and other arbitrary-precision libraries (phase 10 implements arbitrary-precision integers with the standard library only)
- GUI, IDE integration, and tools other than a debugger
- Dependencies other than the C++20 standard library and CMake (no ICU, Boost, or GMP)
- Dependencies on Windows-only or macOS-only APIs. OS-dependent code is isolated in `src/platform/` and kept within one file

## 2. Deliverables and supported environments

The deliverables are three executables and one library. All are built from a single CMake project.

| Deliverable | Contents | Form |
| --- | --- | --- |
| `libmosa` | Runtime containing term representation, unification, the solver, built-in predicates, streams, and module management | Static library |
| `mosa` | Interactive interpreter. REPL, `consult/1`, command lines such as `-g Goal -t halt`, script execution | Executable |
| `mosac` | Prolog → C++20 translator. Goes all the way from a Prolog file to a standalone executable in one step (subcommands build / run / emit / dist / toolchain). C++ source output is an intermediate stage | Executable |
| `mosatest` | Test runner. Runs own Prolog tests, the conformance and differential suites, manual examples, round-trip checks and benchmarks; prints failures to standard error and writes the generated Markdown/JSON reports used by the badges | Executable |

**Build environments (CI must pass on all three OSes)**

| OS | Compiler | Notes |
| --- | --- | --- |
| macOS 26 or later (Apple Silicon) | Xcode 26 or later, or its Command Line Tools (Apple Clang 17) | Primary development environment |
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
5. Arithmetic (chapter 9): integers have arbitrary precision (phase 10, ADR 0025; the flag `bounded` is `false`), floats are IEEE 754 double precision. Provide the `max_integer`/`min_integer` flags; an integer result has no overflow, and one too large for the heap stops at the heap limit. Includes integer division, remainder, bitwise operations, trigonometric functions, `copysign`, and the functions added in Cor.2 such as `truncate`
6. All built-in predicates (chapter 8). The list and stages are given in section 7
7. Flags (7.11): `bounded` (`false`), `max_integer`, `min_integer`, `integer_rounding_function`, `char_conversion`, `debug`, `max_arity`, `unknown`, `double_quotes`. In addition, the implementation-defined flags `encoding` (always `utf8`), the read-only `version` (the integer `Major*10000 + Minor*100 + Patch`), `japanese_punctuation` (section 4), the read-only `dialect` (`mosa`) and `version_data` (`mosa(Major, Minor, Patch, [])`), and the read-only `iso` (`true`). The flag `debug` (7.11.2; the sub-clause is unconfirmed, and that the standard leaves its meaning to the implementation is from the design, not from a source read) has a meaning from phase 14 (ADR 0028): `set_prolog_flag(debug, on)` does what `debug/0` does and `off` what `nodebug/0` does, and `current_prolog_flag(debug, V)` shows the debugger's switch
8. Directives (7.4.2): `dynamic/1`, `discontiguous/1`, `multifile/1`, `op/3`, `char_conversion/2`, `initialization/1`, `set_prolog_flag/2`, `include/1`, `ensure_loaded/1`. Outside 13211-1, the conditional compilation directives `if/1`, `elif/1`, `else/0` and `endif/0` (phase 13, "Conditional compilation" below)

**Grammar rules (phase 8)**

A clause `Head --> Body` in a Prolog text is translated to an ordinary clause when the text is loaded, and `phrase/2,3` are in `system`. 13211-1 does not define grammar rules; the reference is the grammar-rule part of the standard, ISO/IEC 13211-3 (for its source text see `docs/iso-notes.md`, "13211-3"). ADR 0024 fixes the translation. Phase 9 adds `Name//Arity` in export lists, import lists and the directives `dynamic/1`, `discontiguous/1` and `multifile/1` (it means `Name/(Arity+2)`), and `*->` in a grammar body. `expand_term/2` is added in phase 9 in `library(dcg)` (it translates one grammar rule; `term_expansion/2` and `goal_expansion/2` stay out of scope).

**Conditional compilation (phase 13, ADR 0027)**

13211-1 has no conditional compilation. The four directives below follow the SWI-Prolog manual (4.3.1.2 Conditional compilation, the SWI-Prolog 10.1.20 version, read on 2026-11-16), except where iso-notes "Conditional compilation (not in 13211-1)" says otherwise. They work on every path that reads a text: the command line and `consult/1`, `[File]`, `use_module/1,2` and `ensure_loaded/1` as goals and as directives, the directives `reexport/1,2`, `include/1`, libraries, `Machine::load_program`, and the load at start-up of an executable built by `mosac build` (section 6).

| Directive | Meaning |
| --- | --- |
| `:- if(Goal)` | Opens a new conditional block. Unless an outer block is being skipped, calls `Goal`: on success the terms after it are read, on failure they are skipped up to the next `elif`, `else` or `endif` of the block |
| `:- elif(Goal)` | If no earlier branch of the same block was taken, calls `Goal`, and on success the terms after it are read. If an earlier branch was taken, `Goal` is not called and the terms are skipped up to `endif` |
| `:- else` | If no earlier branch of the block was taken, the terms after it are read |
| `:- endif` | Closes the conditional block |

- A condition is solved once in the module being loaded, as a general directive `:- G` is (iso-notes "7.4.2 Other directives (`:- G`)"): when that module is not `user`, it is called as `M:Goal`. Its choice points are cut and its bindings undone. `Goal` is called as written: SWI-Prolog passes it through `expand_goal/2` first, and Mosa has no goal expansion
- A condition that fails counts as false. A condition that throws fails the load with its Formal as the diagnostic, as a general directive does, so a variable `Goal` gives `instantiation_error` and a number gives `type_error(callable, _)`; `halt/0,1` in a condition stops the load at once, as in a general directive. SWI-Prolog instead prints the error and treats the condition as false (iso-notes)
- A skipped part is still read as terms with the current operators and flags, so that nested `if` and `endif` can be counted. Its directives, `op/3`, `dynamic/1`, `module/2`, `include/1` and `use_module/1,2` included, do not run, and its clauses are not added: a skipped part changes no operator, flag, clause, module or import. The test is made before anything else looks at a term, so a skipped term reaches neither `mosatest`'s capture of `test/3` nor the translation of grammar rules, whose errors it cannot raise
- A syntax error in a skipped part is passed over without a diagnostic, and reading goes on after the next full stop (iso-notes question 3, decision A); the translation load of `mosac` does the same. If such an error swallows the `:- endif`, the text ends with the block still open and the load fails, as below. A syntax error in a branch that is read fails the load, as anywhere else
- `if` and `endif` pair within one file (one text being read): a file read by `include/1` must close the blocks it opens, and a block opened in one file cannot be closed in another. The load fails with a diagnostic under its own message key for an `elif`, `else` or `endif` with no open `if`, an `elif` after `else`, a second `else`, and an `if` still open at the end of the text. These structural errors fail the load also while a part is being skipped, in a block nested inside a branch not taken as well (iso-notes)
- Only the four shapes `if/1`, `elif/1`, `else/0` and `endif/0` are conditional directives. Any other arity is a general directive as before, so `:- if(C, T, E)` still calls the built-in `if/3`. Before phase 13 these four directives called a user predicate `if/1`, `elif/1`, `else/0` or `endif/0`, so this is a change in behavior and goes in the CHANGELOG. The four are not goals: `?- if(true).` at the prompt still gives `existence_error(procedure, if/1)`
- `:- module/2` must still be the first term of a file, and a conditional block does not change this rule: when a file starts with `:- if`, a `:- module/2` inside the block is the same error as any `:- module/2` that is not the first term

**Decisions on implementation-defined matters**

| Matter | Decision |
| --- | --- |
| `max_arity` | 1024 |
| Default of `double_quotes` | `codes` (ISO default). Can be changed per module |
| Default of `unknown` | `error` |
| Character range | Unicode scalar values U+0000–U+10FFFF (surrogates are not allowed) |
| String type | None. `"..."` follows the `double_quotes` flag (SWI's string type is a non-goal) |
| Float format | Shortest round-trip representation (`std::to_chars`). Always contains `.`, exponent written with `e`. NaN is written `1.5NaN` and infinities `1.0Inf` and `-1.0Inf`, as in SWI-Prolog and ECLiPSe, and the reader reads them back (iso-notes 6.4.5, review L22) |
| Integer syntax | `0b`, `0o`, `0x`, `0'c`, `_` digit separators (not specified in Cor.3, but allowed) |
| Integers | Arbitrary precision; `bounded` is `false` (ADR 0025). `max_integer` and `min_integer` stay 2^63−1 and −2^63 (iso-notes 7.11.1.2, default A). `^` and `<<`, and `+`, `-`, `*`, `/\`, `\/`, `xor` and `\` with a Big argument, estimate the result's size first and stop with the heap limit before computing it when the estimate exceeds the heap left; the other operations (such as `//`, `rem`, `div`, `mod`, `gcd`, `>>`, `-` and `abs` of one argument, and the copies of Big operands) compute first and stop with the heap limit when the result is stored. An integer beyond the double range raises `evaluation_error(float_overflow)` wherever evaluation turns it into a float: with a float in `+`, `-`, `*`, `/` and `^`, in `**` and the float functions, in `float/1`, and with a float in an arithmetic comparison and in `min/2` and `max/2` (iso-notes 8.7, default A); `/` of two integers raises it only when the quotient is beyond the double range; the standard order compares it with a float without an error |

**Intentional deviations from ISO (each is behind a flag, off by default, or visible only after an explicit import; the read-only `iso` flag is `true` and states that the core semantics follow ISO)**

- Modules (section 5). 13211-2 has few implementations and known inconsistencies, so only its syntax is adopted; the semantics follow SWI-Prolog
- Acceptance of full-width symbols (section 4, off by default)
- Widely used non-ISO predicates such as `format/2,3`, `between/3`, `succ/2`, `msort/2`, `forall/2`, `nb_getval/2` are placed in modules such as `library(lists)` and `library(apply)`. They are not visible by default in the `user` module and are imported with `use_module`

There is no separate strict-conformance mode that disables these deviations. The read-only `iso` flag states that the core semantics follow ISO; it is not a mode switch (owner decision A in iso-notes, Flag iso).

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
- Standard input/output works in UTF-8 on Windows as well. `src/platform/console.cpp` calls `SetConsoleCP(65001)` and `SetConsoleOutputCP(65001)`, and console input is received as UTF-16 via `ReadConsoleW` and converted to UTF-8. Before anything is read or written, the commands and the executables `mosac` builds put stdin, stdout and stderr in binary mode, as on macOS and Linux: a newline is written as LF alone, a 0x1A byte does not end the input, and a CRLF from a pipe reaches the program as CR and LF (review L8; iso-notes "7.10.2 and 8.12"). A program that embeds libmosa keeps the modes it set. Files are opened through `std::filesystem::path` and must support Japanese path names
- `syntax_error` messages from `read_term/2,3` include the line number, the column number (in code points), and the offending token. Columns count one column per character, including full-width characters
- `~t~|` and `~+` column alignment in `format/2,3` takes East Asian width into account (`East_Asian_Width` W/F count as width 2). This is also included in the generated tables

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

1. The `system` module (all built-in predicates and control constructs; redefining one raises `permission_error(modify, static_procedure, PI)`)
2. The calling module's own definitions (local predicates and imported predicates share one namespace; a conflict raises `permission_error(import_into, ...)` at load time)
3. That module's default import source (normally `user`; the default for `user` is `system`), following the chain of default import sources (ADR 0006; `resolve_pred` in `src/module/module_table.cpp`, with the `system` step done by the built-in dispatch before it)

- `Module:Goal` runs `Goal` in the context of `Module`. If `Module` is unknown, `existence_error(module, Module)`
- Predicates created by `dynamic/1`, `assertz/1`, etc. belong to the module that declared or added them. They can also be qualified, as in `assertz(家系:親(a, b))`
- Arguments specified with `:`, `0`–`9`, `^`, or `//` in `meta_predicate/1` are wrapped as `呼び出し元モジュール:引数` (calling module:argument) at call time. This makes `maplist/2,3`, `findall/3`, `catch/3`, `forall/2`, etc. work correctly across modules
- `multifile/1` takes the form `モジュール:PI` (Module:PI) and lets other modules add clauses
- The `user` module is the top-level default. `library(...)` is searched in `$MOSA_LIB` and in `lib/` relative to the executable. An executable built by `mosac` first looks in the list of libraries bundled into it (Section 6) and searches those two places only for a library that is not bundled
- `op/3` is local to the module that declares it. Writing `op(P, T, N)` in the export list makes it effective in importing modules too. Operators declared in `user` are visible in all modules, except while a file found through `library(...)` is read: it is read with the ISO core and its own module's operators (ADR 0006, amendment 1)
- The flags `double_quotes` and `japanese_punctuation` are saved and restored per module (more precisely, per file being loaded)

**Loading from a goal, `reexport/1,2` and module introspection (phase 12, ADR 0026)**

- `use_module/1`, `use_module/2` and `ensure_loaded/1` can also be called as goals, at the prompt too. Each means the same as the directive of the same form: `Spec` is `library(Name)` or an atom, `Imports` is a list of PIs (`as` included) or `except(List)`, and the argument of `ensure_loaded/1` is an atom. A file is still read once: a file already prepared is not read again, and only the imports are added. There is still no autoload, only `module/2` creates a module, and `use_module` accepts module files only (iso-notes, section 5)
- A goal load (`consult/1` and the three above) opens a relative path from the current working directory, even when the goal is called from a directive of a file being loaded; a directive `:- use_module(...)` still opens it from the directory of the file being loaded. The imports go into the context module of the goal: `user` at the prompt, `m` in the body of a clause of module `m`
- A goal load is one outer load (ADR 0026): a failed load is rolled back (ADR 0017), and the reconsult marks (ADR 0020) and the `initialization/1` goals apply as in a directive load; called during a load, it is a nested load. A file found through `library(...)` is read with the ISO core and its own module's operators (ADR 0006, amendment 1). On success the goal succeeds deterministically
- A load that stops at the heap limit (a C++ exception, which the embedding API passes on to its caller and the toplevel reports; at the prompt, a goal or a directive that stops this way is reported and the prompt goes on) is rolled back as a failed load is, from every entry: `Machine::consult`, `Machine::load_program`, a directive load and a goal load. A file whose `initialization/1` goal stops this way keeps what it read, as when that goal raises an error, unless a load that is still reading its text encloses it: the exception leaves that load's reading, so that load is undone with every file it read. The goal of a file loaded by `:- use_module`, `:- ensure_loaded` or `:- reexport` runs after the loading file has been read; that of a file loaded by `:- consult(File)`, `:- [File]` or a goal load inside a directive runs while the loading file is still being read
- An error in the arguments is thrown as an exception, with the Formal the directive gives (`instantiation_error`, `type_error(atom, _)`, `type_error(list, _)`, `type_error(predicate_indicator, _)`). Any other failure writes the diagnostic to `user_error` when the load fails, also inside `;`, `findall/3` and `\+`, and the goal fails (iso-notes question 1, decision A). The text is the one `mosa F` prints: an error inside the opened file is `F: reason`, or `F:Line:Col: reason` for a syntax error, where `F` is the opened path; an error after the file is read (not a module, an import conflict, a predicate that is not exported) is also `F: reason`; an error before a file is found (a library that is not found) starts with the text of `Spec`, as in `library(nope): existence_error(source_sink,library(nope))`; a file that cannot be opened gives only the `cannot_open_file` text, as `consult/1` does. When the whole query then fails, the toplevel does not write the same diagnostic again. `Machine::diagnostic()` still returns the diagnostic of the last load, which is empty when the load stopped with `halt/0,1`, since a halt is not a failure, and when it stopped at the heap limit, since the exception carries the reason (`include/mosa/machine.hpp`)
- Consulting a file again under a choice point is still refused (`take_over`, ADR 0017; iso-notes question 3, decision A), and the refusal is written by the rule above: `consult(F), findall(x, consult(F), L)` gives `L = []` and writes `F: 実行中の述語は置き換えられない` ("F: cannot replace a predicate that is running", the key `predicate_running`) to `user_error` once
- `:- reexport(Spec)` and `:- reexport(Spec, Imports)` are directives only. `Spec`, `Imports` and their errors are those of `use_module/1,2`. The file is loaded and imported as by `use_module`, and the imported predicates are added to the export list of the module being loaded. `reexport/1` and `except(...)` also export the exported operators of the source module (the rule by which `use_module/1` brings operators in, iso-notes "Section 5 Operators in modules"); a list of PIs exports no operators. A predicate given an alias with `as` is exported under the alias
- An export entry holds the module that defines the predicate and its name there (ADR 0026). So an importer of a re-exported predicate imports it from the defining module, `resolve_pred` reaches the definition in one step however many re-exports lie between, and `predicate_property(P, imported_from(M))` gives the defining module. Importing the same predicate along two paths (`lists` directly, and `lists:append/3` through a module that re-exports it) is not a conflict, because the import sources are compared by the defining module (iso-notes "Section 5 Conflict between two imports"). A different predicate under the same name is still `permission_error(import_into, procedure, From:N/A)`
- In a text that is not a module file (read into `user`), `reexport` acts as `use_module`; `user` has no export list, so nothing is added to one
- `mosac`: a file opened by `reexport` enters the bundled closure as a file opened by `use_module` does. After the translation load, the translator also looks for the goals `use_module(library(Name))` and `use_module(library(Name), _)` written literally anywhere in the terms read, which covers clause bodies, general directives `:- G` and `initialization/1` goals, and bundles those libraries, so the executable finds them in its bundled list (iso-notes question 2, decision A). A `Name` that is a variable is not bundled and is searched at run time in `$MOSA_LIB` and `lib/`; a literal name with no library is skipped without a message, and the translation goes on. Section 6 ("Conditional compilation under `mosac`") extends this search to the terms skipped in the translation load and to the forms `reexport(library(Name))` and `reexport(library(Name), _)`

The module introspection predicates are in `system`. 13211-1 has no clause for them and their place in 13211-2 is not checked; their behavior is a subset of the SWI-Prolog 10.0.2 documentation.

| Predicate | Mode | Meaning | Errors |
| --- | --- | --- | --- |
| `strip_module/3` | `strip_module(?Term, ?Module, ?Plain)` | Takes off the leading `M:` of `Term` while `M` is an atom. `Module` is the innermost such `M`, or the context module when there is no `M:`; `Plain` is the rest | `resource_error(cyclic_term)` when the `M:` layers form a cycle (iso-notes 7.3.3, the owner's decision A of 2026-11-17); none otherwise |
| `context_module/1` | `context_module(?M)` | The context module: `user` at the prompt, `m` in the body of a clause of module `m`, the caller's module in a goal called as a `meta_predicate` argument | None |
| `module_property/2` | `module_property(?M, ?P)` | With `M` unbound, enumerates the modules in the order of `current_module/1`. Only a module created by `module/2` has properties; `user` and `system` have none (`module_property(user, P)` fails). The properties are `file(F)` (only a module loaded from a file; `F` is the atom of `Module::file`; in an executable built by `mosac`, `F` is the file's embed key resolved against the working directory, which is `library/Name.pl` for a bundled library and the path relative to the main file's directory for the program's own files, iso-notes), `exports(L)` (the `N/A` of the exported predicates in the order they were exported, re-exported ones included) and `exported_operators(L)` (a list of `op(P, T, N)`, only when the module exports operators) | `type_error(atom, M)`, `domain_error(module_property, P)`, `existence_error(module, M)` (as for `predicate_property/2`, decision A), checked in this order: the type of `M`, the domain of `P`, whether `M` exists |
| `import/1` | `import(+M:PI)` | Imports the exported predicate `PI` of `M` into the context module, by the rule for one item of the PI list of `use_module/2`. It takes one PI, not a list, and not `PI as Alias`: `import(M:(N/A as Alias))` gives `type_error(predicate_indicator, N/A as Alias)` (the row `imp_as` of `tests/pl/modules.pl`). With no `M:`, the source is the context module itself, so it succeeds and changes nothing (an import from a module into itself, as `apply_imports` treats `from == into`). With `M:`, a variable `M` gives `instantiation_error` and a non-atom `M` gives `type_error(atom, M)`; `M1:M2:PI` imports from the inner `M2` | `instantiation_error`, `type_error(predicate_indicator, PI)`, `type_error(atom, M)`, `existence_error(module, M)`, `existence_error(procedure, M:N/A)` (a predicate that is not exported; iso-notes "Section 5 Importing a predicate that is not exported"), `permission_error(import_into, procedure, From:N/A)`, and `resource_error(cyclic_term)` when the `M:` layers form a cycle (iso-notes 7.3.3), checked in this order: the form of each `M:` layer and whether the layers come back to one already taken off, the form of the PI, whether the innermost `M` exists, then the import |

- `predicate_property/2` changes only in the value of `imported_from(M)`; `current_predicate/1` does not change
- New names in `system`: `use_module/1,2`, `ensure_loaded/1`, `strip_module/3`, `context_module/1`, `module_property/2` and `import/1` become built-in predicates. By step 1 of the resolution rules, a clause for one of these names in any module gives `permission_error(modify, static_procedure, PI)`. Before phase 12 a program could define one of them, for example `user:import/1`, so this is a change in behavior and goes in the CHANGELOG. On 2026-11-15 no clause in `tests/conformance`, `tests/pl`, `bench`, `lib` or `docs/manual` had one of these names as its head
- Not provided: `@/2`, `add_import_module/3` and `delete_import_module/2` (iso-notes "Section 5 `@/2` and `add_import_module/3`"). `reexport/1,2` and `[File]` stay directives

**Handling in the translator (`mosac`)**

- Each module corresponds to the C++ namespace `mosa_gen::m<mangled module name>` (for example `mosa_gen::muser`). Module names in UTF-8 cannot be used as C++ identifiers as-is, so they are mangled as `m` + the name with ASCII letters and digits kept, `_` written as `_5f`, and every other byte escaped as `_xx` (two lowercase hex digits). C++ reserves identifiers containing a double underscore ([lex.name]), and this scheme never produces one (project owner's decision B, 2026-10-29). The original name is kept as a string constant and registered in the runtime module table
- Generated code does not call other predicates' C++ functions directly. The control constructs (`,`, `;`, `->`, `*->`, `\+`, `!`, `true`, `fail`, `catch/3`, `throw/1`) run in place in the generated function through the `rt::` control entry points, and `is/2` and the arithmetic comparisons (8.6.1, 8.7.1) are called directly as `rt::arith_is` and `rt::arith_compare`. Every other call, to a user, library or built-in predicate, writes a goal frame through `rt::call_goal` and returns to the trampoline, and the callee is resolved at run time by the rules above; it runs as a C++ function only when its translated unit is attached to that predicate (ADR 0009)
- The translation unit is a "set of modules", and the transitive closure of `use_module` is included automatically. `library(...)` modules are linked as pre-translated objects

**Standard libraries to provide first (all written in Prolog; only those requiring a C++ implementation are placed in `system`)**

| Module | Main predicates |
| --- | --- |
| `library(lists)` | `append/3`, `member/2`, `memberchk/2`, `length/2` (ISO core, so placed in system and re-exported), `nth0/3`, `nth1/3`, `last/2`, `reverse/2`, `msort/2`, `sum_list/2`, `max_list/2`, `list_to_set/2` |
| `library(apply)` | `maplist/2..5`, `foldl/4..6`, `include/3`, `exclude/3` |
| `library(between)` | `between/3`, `succ/2`, `plus/3` |
| `library(format)` | `format/1,2,3` (`~w`, `~p`, `~q`, `~k`, `~W`, `~a`, `~d`, `~D`, `~r`, `~R`, `~f`, `~e`, `~E`, `~g`, `~G`, `~s`, `~c`, `~i`, `~n`, `~N`, `~t`, `~\|`, `~+`) |
| `library(strings)` | `atomic_list_concat/2,3`, `split_atom/4`, `upcase_atom/2` (Unicode uppercasing uses simple case mapping only) |
| `library(dcg)` | `expand_term/2` (translates one grammar rule `H --> B`; a non-variable term that is not `-->/2` comes back unchanged; an unbound first argument is `instantiation_error`) |
| `library(japanese)` | `ひらがな_カタカナ/2` (hiragana/katakana), `全角_半角/2` (full-width/half-width), `読み幅/2` (display width calculation). Implemented with the generated tables |
| `library(pairs)` | `pairs_keys_values/3`, `pairs_keys/2`, `pairs_values/2`, `group_pairs_by_key/2`, `transpose_pairs/2`, `map_list_to_pairs/3` (phase 13; no errors are documented; `group_pairs_by_key/2` groups only adjacent keys that are `==`; `transpose_pairs/2` orders the result by the new keys with `keysort/2`; `map_list_to_pairs/3` is `meta_predicate map_list_to_pairs(2, ?, ?)`) |
| `library(assoc)` | `empty_assoc/1`, `list_to_assoc/2`, `ord_list_to_assoc/2`, `get_assoc/3`, `get_assoc/5`, `max_assoc/3`, `min_assoc/3`, `gen_assoc/3`, `put_assoc/4`, `del_assoc/4`, `del_min_assoc/4`, `del_max_assoc/4`, `assoc_to_list/2`, `assoc_to_keys/2`, `assoc_to_values/2`, `is_assoc/1`, `map_assoc/2`, `map_assoc/3` (phase 13; an AVL tree whose term shape is not part of the interface; `list_to_assoc/2` gives `domain_error(unique_key_pairs, List)` when a key repeats, and `ord_list_to_assoc/2` gives `domain_error(key_ordered_pairs, List)` when the keys are not strictly ascending; `gen_assoc/3` enumerates in ascending key order; `del_min_assoc/4` and `del_max_assoc/4` fail on an empty assoc (iso-notes); `map_assoc/2,3` are `meta_predicate` with `1` and `2`; keys must be ground, which is not checked) |
| `library(ordsets)` | `is_ordset/1`, `ord_empty/1`, `ord_seteq/2`, `list_to_ord_set/2`, `ord_intersect/2`, `ord_disjoint/2`, `ord_intersection/2`, `ord_intersection/3`, `ord_intersection/4`, `ord_add_element/3`, `ord_del_element/3`, `ord_selectchk/3`, `ord_memberchk/2`, `ord_range/4`, `ord_subset/2`, `ord_subtract/3`, `ord_union/2`, `ord_union/3`, `ord_union/4`, `ord_symdiff/3` (phase 13; no errors are documented; `ord_intersection/2` fails on an empty list; the behavior on input that is not an ordered set is not defined, as in the documentation) |

The three libraries of phase 13 are written anew, not copied from SWI-Prolog's source. Their predicate names, argument order and documented errors follow the SWI-Prolog manual (the sections `pairs`, `assoc` and `ordsets`, the SWI-Prolog 10.1.20 version, read on 2026-11-16). A use the documentation does not cover, such as an argument that is not a list, gets no added error. An error is thrown as `error(Formal, context(Name/Arity, _))`, as in `library(between)`. `ord_intersect/3`, which that manual marks as deprecated, is not provided.

## 6. Architecture (runtime, interpreter, C++20 translation)

The interpreter and the translator share the same runtime (terms, heap, trail, built-in predicates); the only difference is "how clauses are executed". This lets differential tests guarantee that both produce the same results.

**Processing pipeline**

1. Lexical analysis (`lexer`): UTF-8 → token stream. Uses the character classification from section 4
2. Parsing (`reader`): operator precedence parser (Pratt method). Full support for the `read_term/3` options (`variable_names`, `singletons`, `quoted`, etc.)
3. Loading (`loader`): directive processing, conditional compilation (section 3, phase 13), module resolution, and translation of grammar rules (`term_expansion/2` and `goal_expansion/2` are out of scope; see section 3 and ADR 0024)
4. Clause compilation (`clause_compiler`): lowers clauses to the IR (intermediate representation). The IR is the common input to the interpreter and `mosac`
5. Execution: the interpreter interprets the IR directly. `mosac` generates C++20 from the IR

**Core of the runtime (`libmosa`)**

| Component | Design |
| --- | --- |
| Cell representation | 64-bit tagged integer `Cell`. The low 3 bits are the tag: REF, ATOM, INT (61-bit immediate), STR (pointer to a functor cell in the heap), LIS (list), FLT (boxed double), BIG (an integer outside int64: a pointer to a head word and the magnitude words, phase 10, ADR 0025). An int64 that does not fit in 61 bits is boxed like FLT, and `max_integer` stays 2^63−1 |
| Heap | A single contiguous array: address space up to the limit is reserved at start-up and committed from the bottom as needed, so committed cells never move (ADR 0010). Reclaimed on backtracking simply by resetting the top |
| Trail | Pushes the addresses of bound REFs. Conditional trailing (records only cells older than the choice point) |
| Choice point stack | Holds the continuation, trail position, heap position, pointer to the alternative clause, and cut barrier |
| GC | None until Phase 5 (only reclamation by backtracking). A sliding mark-and-compact collector that runs only at safe points between steps is added in Phase 7 (ADR 0010). Near the limit, 3 GCs in a row that each free fewer than `limit / 32` words stop the query with the heap-limit message. Provide `garbage_collect/0` |
| Atom table | UTF-8 byte sequence → 32-bit atom number. Does not need to be thread-safe (non-goal) |
| Exceptions | Prolog's `throw/1` does not use C++ exceptions; it pushes `catch` frames on the choice point stack and unwinds them. C++ exceptions are used only for fatal errors inside the runtime (`resource_error`, such as the heap limit). `resource_error(cyclic_term)` from evaluating a cyclic expression, from a cyclic chain of `M:` layers or from calling a goal whose control structure is cyclic is not fatal: it is thrown like other errors (iso-notes 7.3.3) |
| Streams | A value `Stream` struct in a vector (no abstract class, ADR 0005), tagged by kind for files / standard I/O / memory (equivalent to `open_string`); file input is pulled in chunks from an open `std::ifstream` as needed. Buffering and peeking (`peek_char/1`) are built in |

**Execution model: binarization + explicit continuations**

A clause `h :- b1, b2, b3.` is transformed (binarized) into a form with the continuation as the last argument, `h(K) :- b1(b2(b3(K)))`. This removes the need for the WAM environment stack and makes translation to C++ simple functions and loops.

- Each predicate corresponds to a C++ function of the form `void p_X_N(Machine::Impl& machine, std::uint64_t frame, std::uint32_t state)` (`rt::NativeProc`, ADR 0009). `frame` is the goal frame on the heap, which holds the arguments and the continuation; `state` 0 is the call and the other states redo a clause or resume after a body goal. Failure is reported through the machine, not a return value
- The execution loop is a trampoline: `while (goal = m.pop()) { dispatch(goal); }`. C++ recursion is not used, so deep recursion does not overflow the stack
- Choice points are created when switching between clauses inside a predicate body. The clause compiler performs first-argument indexing (type tag + functor), and no choice point is created when there is a single candidate
- The cut barrier is the height of the choice point stack. The runtime sets it before every call and redo of a generated function, and a resumed body keeps it in word 2 of its continuation frame (ADR 0009). `call/N` uses the height at call time as the new barrier, which makes cut local

**C++ generated by `mosac`**

- One `.cpp` per module, plus a header declaring the module's unit function and its predicate functions. Predicate names are `p` + the mangled name + `_` + arity (e.g. `app/3` becomes `papp_3`)
- Generated code includes only `mosa/runtime.hpp` and uses nothing beyond the C++20 standard library
- Each clause is a function, and each resume point in its body (the continuation after a call, and the branch, then, else, done, recovery and exit points of a control construct) is a further function. A per-predicate function dispatches on `state` with a `switch` over `state_of(clause, point)`, numbered in the order the translator creates the resume points (ADR 0009). No `goto` and no macros; use `inline` functions and `constexpr` (to keep generated code readable and traceable in a debugger)
- Only static predicates are translated. `dynamic` predicates and clauses added by `assert` run on the interpreter path (IR interpretation) and coexist on the same `Machine`
- The output includes `main.cpp` (a call equivalent to `initialization(main)`) and a `CMakeLists.txt`, so that `mosac emit app.pl -o out/ && cmake -S out -B out/build && cmake --build out/build` produces an executable
- So that a human can read generated code and trace it back to the original Prolog, the original clauses are inserted as comments at the start of each predicate (they may stay in Japanese)

**`mosac` build and run pipeline (do not stop at emitting C++)**

The default behavior of `mosac` is "take a Prolog file and produce a standalone executable". C++ source is only an intermediate artifact; users never touch CMake or the compiler directly.

| Subcommand | Behavior | Output |
| --- | --- | --- |
| `mosac build app.pl -o app` | Translate → compile → link in one step. The default | Executable `app` (`app.exe` on Windows) |
| `mosac run app.pl -- 引数...` | build, then run immediately. Built in `./.mosac-cache` like `build`; a copy of the executable runs from a temporary directory (kept with `--keep`), and the exit code is returned as-is | Execution result |
| `mosac emit app.pl -o out/` | Outputs only C++ source and `CMakeLists.txt` (for people who want to modify it by hand, and for debugging) | `out/` |
| `mosac dist app.pl -o dist/` | build + distribution package: one folder `<stem>-<os>-<arch>/` with the executable, Mosa's LICENSE and NOTICE, and README.txt (ADR 0023) | `<stem>-<os>-<arch>.tar.gz` (`.zip` on Windows) |
| `mosac toolchain` | Shows the location and version of the detected compiler, CMake, and bundled runtime. If there is a problem, prints remedies in Japanese | Diagnostic output |

(`引数...` in the `run` row means "arguments...".)

**Toolchain handling**

- The C++ compiler is searched for in this order: `--cxx` → environment variable `MOSA_CXX` → `CXX` → OS default (`clang++` on macOS; `g++` then `clang++` on Linux; on Windows, find MSVC with `vswhere`, and `mosac` itself sets the environment variables equivalent to `vcvarsall`). If none is found, print the same diagnostics as `mosac toolchain` and stop, showing the installation steps for each OS (Xcode Command Line Tools, `apt install g++`, Visual Studio Build Tools). A compiler named with `--cxx`, `MOSA_CXX` or `CXX` runs in the current environment without `vcvarsall`, so a named `cl` needs a Developer Command Prompt (owner decision, 2026-10-30)
- CMake is optional. Use it if present; otherwise invoke the compiler directly (compile the generated code file by file, and `mosac` itself constructs the link command). The minimum requirement on the user side is therefore "one C++20 compiler is installed"
- The runtime `libmosa` is shipped as a prebuilt static library and headers in the `mosac` installation directory (`<prefix>/lib/mosa/<OS>-<arch>-<compiler>/`, with `<OS>` one of `macos`, `linux`, `windows`, `<arch>` one of `arm64`, `x86_64`, and `<compiler>` the family `clang`, `gcc` or `msvc`). `mosac` finds that directory from its own location: `<prefix>/bin/mosac` looks in `<prefix>/lib/mosa/<key>/`, and in the build tree the directory of `mosac` itself holds `libmosa`, `include/` and `pl/`. Only the Release build is installed, so `mosac build --debug` links the Release runtime. The runtime is not rebuilt on the user's machine every time. As a fallback when the ABI does not match, `--runtime-source` builds it from the bundled source `<prefix>/lib/mosa/source/` with CMake (the result is cached in `.mosac-cache/runtime/<hash>/`)
- The standard libraries in `lib/*.pl` are shipped as pre-translated objects, one per library (`pl/libmosa_pl_<name>.a`, `pl/mosa_pl_<name>.lib` on Windows, next to `libmosa`), and only those loaded transitively with `use_module` are linked, counting the libraries named literally as `use_module(library(Name))` or `use_module(library(Name), _)` in goals (Section 5), plus `messages_ja` and `messages_en` for the text of uncaught errors. The source text of each linked library is embedded in the executable as well, so the library is consulted from the executable and its unit attaches to it, never from a file. The interpreter's copies stay in `<prefix>/bin/lib/*.pl`
- The default is static linking with `-O2`. On Windows, `/MT`. The resulting executable needs neither a C++ runtime DLL, nor `mosac`, nor any Prolog files on the target machine. `--debug` enables `-O0 -g` and runtime clause tracing. The bundled library units are prebuilt without tracing, so `--debug` traces the program's own units only
- Intermediate files are placed in `.mosac-cache/<hash>/` in the current directory and reused based on hashes of the Prolog source and the runtime. The generated C++ always stays there; `--clean` deletes `.mosac-cache/` before building, and `mosac run --keep` keeps the temporary directory that holds the copy of the executable
- Cross-compilation is a non-goal. Executables are built only for the OS and architecture on which `mosac` runs

**Error handling (reported differently depending on whose responsibility it is)**

- Problems in the user's Prolog (syntax errors, export inconsistencies) → stop at the translation stage and report with Prolog line numbers. The compiler is not started. An exported predicate with no definition is only a warning in a module that has a conditional branch not taken at build time ("Conditional compilation under `mosac`" below)
- Calls to predicates that are defined nowhere → report each as a warning with its Prolog line number and translate anyway. The executable raises `existence_error` when the goal runs, or follows the `unknown` flag, as `mosa` does (review L12; iso-notes 7.7.7)
- Generated C++ fails to compile → treated as a `mosac` bug. Print only "内部エラー: 生成コードがコンパイルできません。`.mosac-cache/<hash>/` を添えて報告してください" (Internal error: the generated code cannot be compiled. Please report it with `.mosac-cache/<hash>/` attached), and show the raw compiler errors only with `--verbose`. In CI, "generated C++ always compiles" is an invariant, checked across all differential test cases
- Compiler not found, insufficient disk space, permissions → report as environment problems, with remedies

**Behavior of the resulting executable**

- If `:- initialization(main, main).` is present, it is the entry point. If `main/0` succeeds, the exit code is 0; if it fails, 1; on an uncaught exception, print a message and exit with 2. The argument of `halt/1` becomes the exit code as-is
- Command-line arguments are received through the flag `argv` (a list of atoms)
- An executable built with `mosac build --interactive` starts a REPL with the translated code already loaded (translated code and the interpreter coexist as described above). `--interactive` is accepted by `build` only (`-o` works as usual; `mosac run` rejects it). It skips `:- initialization(main, main).` and starts the REPL (owner decision, 2026-10-30)
- The executable requires no external files at startup. The libraries it bundles are already linked, and `use_module(library(X))` resolves against the bundled list before the search of Section 5. A library that is not bundled, such as one whose `Name` is a variable in the program text, falls through to that search at run time: `$MOSA_LIB`, then `lib/` next to the executable

**Conditional compilation under `mosac` (phase 13, ADR 0027; iso-notes question 1, decision A)**

- The translation load runs declarations only and does not call a general directive `:- G` (iso-notes "7.4.2 Other directives (`:- G`)"). The conditions of `if` and `elif` are the exception: they are the only user goals the translation load calls, and they are solved as in section 3. The branches taken decide which clauses are translated, and the `use_module`, `include`, `ensure_loaded` and `reexport` of those branches bring files into the bundled closure along the same paths as before
- In the translation load, a condition that throws or calls `halt/0,1` only means that its branch is not taken: the translation does not stop, and the diagnostic and the halt request that the condition left are dropped. The executable's own load decides, as in `mosa`: there a condition that throws fails the load, and a condition that calls `halt/0,1` stops the load at once (section 3) and ends the program with the exit code of the halt
- The executable's load at start-up evaluates every condition again, so it takes the branches that `mosa` would take in the same run. When it takes a branch other than the translation's, the predicates whose fingerprint does not match and the predicates that were not translated run as IR (ADR 0009): the result is the same as `mosa`'s, only slower, and no warning is given. The entry point of `initialization(main, main)` is the one that the load at start-up records, so one inside a branch counts only when that branch is taken at start-up
- The search for libraries named literally in goals (section 5) also covers the terms skipped in the translation load, and in every term it takes `reexport(library(Name))` and `reexport(library(Name), _)` as well as the two `use_module` forms. A library found this way is bundled when it exists and skipped without a message when it does not, so a library is bundled in whichever branch it is named

Where the translation load reads more than the load at start-up, the executable keeps `mosa`'s result as follows:

- The unit of a module file of the program other than the main file attaches only when the load at start-up has read that file. The executable does not read a bundled module file on its own, and attaches no translated code to a file it did not read, so the directives and initialization goals of a module that only a branch not taken at start-up loads do not run and its exported predicates are not imported, as in `mosa`. In an executable whose program has no conditional block, every file of such a unit has been read once the main file's load succeeds, so its behavior does not change
- A module that the translation load first read inside a conditional branch, a program file or a library, is not bound in place by the generated code: a `M:G` into it goes through `:/2` at run time, which gives `existence_error(module, M)` when the load at start-up did not load `M`, as in `mosa`. A file opened while the condition of an `if` or `elif` is being evaluated counts as opened inside a branch, since at start-up the condition may give another result, and libraries named literally in the conditions of `if` and `elif`, solved at build time or not, are bundled when they exist
- In the translation load, a branch not taken that holds a directive counts as a skipped general directive, because its `op/3` could change how later terms read: a later syntax error does not stop the build, and the file runs as IR, leaving the verdict to the load at start-up. For example, `:- dynamic(f/0). :- assertz(f). :- if(f). :- op(700, xfx, ===>). :- endif. a ===> b.` builds: the translation load does not run `assertz(f)`, so there `f` fails and the branch is not taken, while at start-up `f` succeeds, the operator is defined, and `a ===> b` is read with it
- In the translation load, a directive or clause that fails inside a branch taken at build time, or a syntax error there, does not stop the build when a goal was skipped before the condition of that branch was solved: a general directive not called, or a directive in a branch not taken, in that file, in a file read before it, or in a file that the condition itself read; for an `elif` or `else`, the earlier branches of its own block do not count, since start-up never reads that `elif` or `else` after taking one of them. That skipped goal may change the condition at start-up, so the file (and the file that read it, when the file has no module of its own, as a part read by `include/1`; for a clause with a qualified head `M:H`, also the file that declares `M`), and the files the failing directive opened, run as IR, and the load at start-up decides, as in `mosa`. An exported predicate that such a failure leaves undefined does not stop the build, also when another module reexports it, since a reexport is checked against the file of the module that defines it. For example, `:- dynamic(skip/0). :- assertz(skip). :- if(\+ skip). :- use_module(no_such_helper). :- endif.` builds, and the executable does not take the branch at start-up. The same directive outside every branch, or in a branch whose condition came before any skipped goal, still stops the build, since no skipped goal can change that condition. A syntax error or a refused clause in a file that skipped a goal earlier itself is passed over wherever it is (the item above, and iso-notes "7.4 Clauses for built-in predicates and control constructs, and bodies that are not callable"), so for them the earlier branches of an `elif` or `else` count too. Once a block whose condition was solved after a skipped goal has closed, a clause later in the load that cannot be added (clauses apart, a clash with an imported predicate) does not stop the build either, since the branch taken at build time, or the one skipped there (with its `discontiguous/1`, say), may be the cause: the file runs as IR and the load at start-up decides. A clause whose form is wrong (`a --> 1.`) and a directive that fails there still stop the build (I2 fix round 4)
- An exported predicate with no definition stops the build ("Error handling" above), except in a module whose file has a branch not taken at build time, where it is a warning with the same text, since that branch may define it and the executable then behaves as `mosa` does. A file has a branch not taken when a condition skipped at least one of its terms (an empty branch does not count), and the mark passes up to every file that reads it, through `include/1`, `use_module` and libraries alike, so a module that reads such a file also gets the warning; a reexported predicate is also judged by the file of the module that defines it, so a second module that reexports a module already read gets the warning too. Only the verdict of the build changes, not the results of the executable

Known limits (also written in the CHANGELOG and iso-notes):

- A file named by a plain path (`include(foo)`, `use_module(foo)`, `ensure_loaded(foo)`) inside a branch not taken at build time is not bundled. If the executable takes that branch at start-up, it reads the file from the working directory, as it does for a file that a goal load names by a plain path (phase 12)
- The conditions run at build time too, so a condition that writes output writes it in the output of `mosac build` as well
- A call to a predicate that only a branch not taken at build time defines gets the undefined-call warning of `mosac`
- The conditions are solved where `mosac build` runs. A condition that reads the files there or the flag `argv` can take a branch that the executable would not take where it runs; a directive or clause that fails in that branch stops the build, as it would stop `mosa` in the same place, unless the I2 item above leaves it to start-up
- A condition that calls `halt/0,1` stops the load at start-up (section 3), but in the translation load it only means that its branch is not taken, so a directive or clause after it that fails can stop the build, although `mosa` and the executable halt before reaching it

**Source layout**

| Directory | Contents |
| --- | --- |
| `src/core/` | Cell, heap, trail, atoms, unification |
| `src/read/` | lexer, reader, operator table, Unicode tables |
| `src/write/` | `write_term/3`, `writeq`, float formatting, quoting decisions |
| `src/machine/` | Execution loop, choice points, catch/throw, IR definitions, clause compiler and indexing (`compile.cpp`, `index.cpp`) |
| `src/builtins/` | Built-in predicates (one file per ISO chapter: `control.cpp`, `arith.cpp`, `atoms.cpp`, `streams.cpp`, ...) |
| `src/module/` | Module table, import resolution, meta\_predicate |
| `src/compile/` | Reserved for the clause compiler; currently empty (the clause compiler is in `src/machine/`) |
| `src/codegen/` | IR → C++20 generation |
| `src/platform/` | OS-dependent code (Windows console, path conversion). Do not write `#ifdef _WIN32` anywhere else |
| `lib/` | Standard libraries written in Prolog |

**Debugger (phase 14, ADR 0028)**

- The ports call, exit, redo, fail and exception are shown only by the `mosa` command, the session that `run_session` runs. The session gives the machine a `DebugConsole` (`src/machine/debug_state.hpp`) that writes the port lines through the session's `Echo::warn` to standard error, where `user_error` also writes (and into `SessionResult.err`), and reads the command lines from where the REPL reads the `;` after an answer. When the standard input is not a terminal and `MOSA_TRANSCRIPT=1` is not set, nothing is read and every port is one that does not stop. `mosatest`, the executables of `mosac build` and an embedded `Machine` have no `DebugConsole`: there the debugger predicates only change state, and answers and output do not change. The clause tracing of `mosac build --debug` (`kTrace`) is a separate thing
- A box is made in the way `catch/3` makes its own. When `step` has read a goal to show, the debugger is on and a `DebugConsole` exists, it pushes a sentinel choice point `Pick::DebugBox`, an exit frame `Ctrl::DebugExit`, and a copy of the goal frame whose word 3 is that exit frame, and runs the copy in the same step (`run_boxed`), so `Impl` holds no address of the copy in the heap, which the garbage collection at the next safe point would make stale. Exit is caught at `DebugExit`, fail when backtracking reaches the sentinel, exception when `propagate_throw` removes a sentinel that has not yet exited (an exited sentinel and `Pick::DebugRedo` are removed silently), and redo by `Pick::DebugRedo`, which is pushed at exit when a choice point is left inside. With the debugger off, or with no `DebugConsole`, `step` costs one branch
- A goal has no box when it is one of: the goals that `launch` handles as IR nodes and the frames of the same names (`true/0`, `fail/0`, `false/0`, `!/0`, `,/2`, `;/2`, `->/2`, `*->/2`, `:/2`, `catch/3`, `throw/1`, `halt/0,1`); `call/1..8`, whose inner goal gets the box; the nine debugger predicates and the predicates whose name starts with `$`; a call whose context module is a library module (the module of a `library(Name)` that was resolved and read), except that `Lib:G` written in a context that is neither a library module nor `system` gives its `G` a box when `G` is one goal; the goals of a control construct after `Lib:` and a `Lib:`-qualified goal that a library predicate calls (as `maplist(lists:append([a]), Ls, Rs)` does) get none (adversarial review of PR #4)
- The commands `r` (retry) and `f` (fail) cut the stack above the sentinel with `trim_stack` and set `failed`, and backtracking does the rest. The cleanups of ADR 0016 run as for a cut, which `run_loop` does before the next step. `r` marks the sentinel as "to reopen", and when backtracking reaches it, it reopens the box without writing a fail port. `f` at an exception port fails the box without a fail port. `nodebug/0` and the command `n` return the way of going to leap, so a `spy/1` after `trace, nodebug` does not start creeping
- In leap only the calls to a spied predicate get a box (owner decision A on question 2, 2026-11-18), so a run up to a spy point is near normal speed and a tail call takes no more memory than usual. The depth and the list of `g` count only the boxes that are open. The spy points are a set in `DebugState::spies`, scanned linearly
- `abort` (the command `a`) is a new stop kind, `StopKind::Abort`: `Impl::abort_now` is seen by `run_loop` right after `halt_now`. `catch/3` does not catch it. Unlike `halt`, each run that it stops first runs the cleanups of `setup_call_cleanup/3` left in it, as a query closed after its answer does (ADR 0016), innermost run first and with no ports; their failures and errors are dropped, the debugger's on/off bit and mode stay as at the abort, and a `halt` in them ends the process (that they run is owner decision A, 2026-11-18; how they run is a default A in `docs/open-items.md`). In a `:- G` or `:- if(G)` directive it makes the load fail as a load error does and rolls it back (owner decision A, 2026-10-29), and the roll-back restores the debugger's on/off bit, mode and leashed ports together (owner decision B, 2026-11-18); in an `initialization/1` goal it follows the rule of that goal (iso-notes 7.4.2): the load fails, the clauses already read stay, and the later initialization goals are skipped. The same holds when the abort stops the initialization of a file that a directive loaded, in either form, `:- [File]` or a directive goal (owner decision A, 2026-11-18). Then the outer query stops. Goals that a built-in launches for itself run in the `system` context or as `'$once'`/`'$not'` and get no box, and a `DebugExit` whose sentinel was cut writes no Exit port. The notices of `spy/1` and the output of `debugging/0` go to the `user_error` stream (in executables too), while the port lines and the notices of the commands `+` and `-` go through the session console
- The cutting of a term at a depth is a function of the writer (`WriteOptions::max_depth`). The option `max_depth/1` of `write_term/2,3` and the port line (depth 10) share it. The option is not in 13211-1 (iso-notes "Debugger (not in 13211-1)")

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
| G | Non-ISO utilities (`library(*)`, the table in section 5) | `format/1,2,3`, `between/3`, `succ/2`, `msort/2`, `sort/4`, `sort/2` and `keysort/2` (ISO core, so in system), `forall/2`, `nb_setval/2`, `nb_getval/2`, `tab/1,2`, `term_to_atom/2`, `numbervars/3`, `setup_call_cleanup/3`, `garbage_collect/0`, `statistics/2` |
| H | Debugger (not in 13211-1; phase 14, ADR 0028) | `trace/0`, `notrace/0`, `debug/0`, `nodebug/0`, `spy/1`, `nospy/1`, `nospyall/0`, `leash/1` and `debugging/0`, all in `system`; and the option `max_depth/1` of `write_term/2,3` (iso-notes "Debugger (not in 13211-1)") |

**Rules of the error system**

- All errors are thrown in the form `throw(error(Formal, Context))`. `Context` is `context(PI, Extra)`, where `PI` is the predicate indicator and `Extra` is additional information (for `syntax_error`, the form `syntax(stream(S, Line, LinePos, CharNo), Token)` rather than `位置(Line, Col)`, with `Token` the offending token as an atom, ADR 0007). ISO permits implementation-defined terms in `Context`
- The order of argument checks strictly follows each predicate's definition in ISO (the order listed in 8.x.y.3 Errors). For example, `atom_length(X, foo)` does not throw `type_error(integer, foo)`; it checks `X` first and throws `instantiation_error`. Order violations are detected by the conformance suite
- On the C++ side, use only the single family of helpers `throw_error(m, Formal, PI)`, and build each kind of `Formal` with functions such as `make_type_error(m, Type, Culprit)`. Writing string literals directly is forbidden (to prevent typos in atom names)
- Arithmetic errors: division by zero is `evaluation_error(zero_divisor)`, float overflow is `evaluation_error(float_overflow)`, and `sqrt(-1)` etc. is `evaluation_error(undefined)`. An integer result does not overflow (phase 10, ADR 0025): one too large for the heap stops at the heap limit, and `evaluation_error(int_overflow)` is left only for rounding an infinite float to an integer. `inf`/`nan` are thrown when the `float_overflow`/`undefined` flag is `error`; otherwise IEEE 754 values are returned (SWI-compatible `float_overflow`, `float_zero_div`, `float_undefined` flags)
- The top level displays uncaught exceptions in the form `警告: ゴールは例外を送出しました: error(...)` (Warning: goal raised exception: error(...)) in Japanese or English (flag `message_language`, whose default is determined from the environment variable `LANG`, or, on Windows when `LANG` is absent or empty, from the Windows display language). Message strings are placed as Prolog facts in `lib/messages_ja.pl` and `lib/messages_en.pl`, and are not embedded in C++

## 8. Test strategy

Tests have four layers. Three numbers, "conformance suite pass rate of the ISO column", "conformance suite pass rate of the library column" and "interpreter/translator agreement rate", are updated as README badges on every commit.

| Layer | Contents | How to run |
| --- | --- | --- |
| 1. C++ unit tests | Components that exist before Prolog starts: Cell, atoms, lexer, unification, UTF-8 conversion, etc. | No external framework; a minimal `assert`-based runner in `tests/cpp/` (`tests/cpp/harness.hpp`, 50 lines or fewer). Registered with CTest |
| 2. Own Prolog tests | `tests/pl/<chapter>.pl`. Written as facts `test(名前, ゴール, 期待)` (name, goal, expected), loaded and run by `mosatest`. The expectation is one of 4 kinds: `true`, `fail`, `error(Formal)`, `solutions([X=1, X=2])` | `mosatest tests/pl/` |
| 3. ISO conformance suite | Logtalk's Prolog conformance tests are placed unmodified in `tests/conformance/logtalk/`. They are Apache-2.0, the same as the repository, and the copyright notices are kept in `NOTICE.txt` there. The INRIA Suite (`inriasuite.pl`) is not bundled because there is no permission to redistribute it. Ulrich Neumerkel's tables are not copied. Run with `mosatest --suite`, writing the pass count / total and the list of failures to `docs/conformance.md` | Every CI run. Changes that lower the pass rate are not merged. As the mechanical part of that rule, `mosatest --suite` compares the rate with the `docs/conformance.json` in the tree before rewriting it and fails (without rewriting the JSON) when the rate went down, on top of the 90% floor; this catches a regression whose badge was not regenerated (the macOS job also diffs the JSON). Lowering the badge itself in the same commit is a visible change to the JSON and is caught in review, not by this comparison. The library column (the library predicates Mosa provides outside 13211-1, counted apart from the ISO rate since phase 11) is compared in the same way with `docs/conformance-library.json`, without the 90% floor |
| 4. Differential tests | All goals from layers 2 and 3 are translated, built, and run with `mosac` run, and compared byte for byte with the interpreter's output (solutions, error terms, standard output) | `mosatest --diff`. Required in CI from Phase 6 onward |

**Rules for writing tests**

- When implementing a new built-in predicate, first copy the standard's Examples (8.x.y.4) verbatim into `tests/pl/`, then implement. Each line of the Examples is one test
- Error tests compare only the `Formal` term, not `Context` (because it is implementation-defined)
- Japanese tests (section 4) are placed in layer 2 as `tests/pl/japanese.pl` and must always be included in the layer 4 differential tests
- Input/output tests use memory streams (`open_string`, and a `system` predicate equivalent to `with_output_to/2`), and temporary files are created only under `tests/tmp/`
- On Windows CI, standard output is captured as UTF-8 for comparison (put `chcp 65001` in the workflow)
- The examples in `docs/manual/en/` and `docs/manual/ja/` are also run with `mosatest --manual` and registered with CTest
- The CTest presets run tests in parallel with a default per-test timeout. Tests that share a scratch directory use resource locks, and tests that measure time ratios run serially. CI also runs GC-stress jobs and a Linux Debug job with AddressSanitizer and UndefinedBehaviorSanitizer; the sanitizer job excludes tests that link generated executables without the sanitizer runtime, timing-ratio tests, and the deepest stack tests

**Benchmarks (lower priority than pass rate, but used to detect regressions)**

- `bench/`: `nrev` (naive reverse, LIPS measurement), `queens`, `zebra`, `tak`, `deriv`, `crypt`, `bigint` (arbitrary-precision integers, phase 10). Measured with both the interpreter and the `mosac` translation, and recorded as a table in `docs/bench.md`
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
| 8 | Grammar rules: `-->` translated when a text is loaded (ADR 0024), `phrase/2,3` in `system`, the same behavior under `mosac` | `tests/pl/dcg.pl` passes in `mosa`, with zero differences in `mosatest --diff`. The pass rate on the bundled Logtalk conformance suite does not go down |
| 9 | ISO completion and robustness: `subsumes_term/2`, `acyclic_term/1`, a reader that runs on frames instead of recursion (ADR 0011 amendment 4), `Name//Arity`, `*->` in a grammar body, `expand_term/2` in `library(dcg)` (ADR 0024 revision 3) | The Examples of 8.2.4.4 and 8.3.11.4 pass in `tests/pl/unify.pl` and `types.pl`; `mosatest --suite` takes `predicates/subsumes_term_2` and `predicates/acyclic_term_1` into its range as directories (the STO items stay in their own column, item by item); the verdict on `subsumes_term` goes through the built-in; the pass rate does not fall below 98.4%; the ctest `stack_deep_read` passes under 1 MiB on 3 OSes; `tests/pl/dcg.pl`, `lib_dcg.pl` and `modules.pl` pass and `mosatest --diff` shows zero differences |
| 10 | Arbitrary-precision integers: the Big box (ADR 0025), unbounded arithmetic, reading and writing, `bounded` is `false` | `tests/pl/bigint.pl` and the tests whose expected values this change corrects pass under `mosa`, and `mosatest --diff` shows zero differences (including `japanese.pl`); `mosatest --suite` takes `unbounded/` into its range, every item that runs there passes or is written in iso-notes as a difference in interpretation that the owner has decided, the pass rate does not fall below 98.4% on the real denominator, and `docs/conformance.md` is regenerated; `tests/pl/bigint.pl` passes under `MOSA_GC_STRESS`, a loop that keeps making Bigs levels off in memory, and `1 << (1 << 62)` stops before it allocates; the interpreter time of `bench/nrev.pl` and `bench/queens.pl` (median of 3 runs, Release, same machine) is at most 1.05 times that of v0.1.4, `mosac`'s `nrev` stays at least 3 times faster than the interpreter, and the values are written in `bench.md` |
| 11 | Conformance scope and `format/2,3`: the ISO directories outside the denominator (`sort_2`, `keysort_2`, `ground_1`, `not_unifiable_2`) in the ISO column, a separate library column for the library predicates Mosa provides, and the `format` directives the bundled suite tests (`~i ~k ~W ~p ~N ~r ~R ~D ~E ~g ~G ~+`) | `mosatest --suite` counts `sort_2`, `keysort_2`, `ground_1` and `not_unifiable_2` in the ISO column (and `length_2` if the owner decides `length/2` is ISO, iso-notes) and the ISO pass rate does not fall below 98.4%; `docs/conformance.md` has a library column that holds every directory the scope rule of the phase 11 design names, `format_2` and `format_3` have no `domain_error(format_directive, _)` failure, every remaining library failure has its reason in `docs/iso-notes.md`, and the library pass rate is at least 98.0%; the new directive tests in `tests/pl/lib_format.pl` and `tests/pl/japanese.pl` pass under `mosa` and `mosatest --diff` shows zero differences (the library directories and `japanese.pl` included); `mosatest --manual --all-tables` passes; CI passes on 3 OSes |
| 12 | Module completion: `use_module/1,2` and `ensure_loaded/1` as goals (at the prompt too), the diagnostic of a goal load that fails, `reexport/1,2`, and in `system` `strip_module/3`, `context_module/1`, `module_property/2` and `import/1` (section 5, ADR 0026) | The new rows of `tests/pl/modules.pl` and `tests/pl/japanese.pl` pass under `mosa` (goal `use_module/1,2` and `ensure_loaded/1`, `reexport/1,2`, `strip_module/3`, `context_module/1`, `module_property/2`, `import/1`) and `mosatest --diff` shows zero differences; a test passes in which a goal load that fails inside `;`, `findall/3` or `\+` writes its diagnostic to `user_error` exactly once (for example, `consult(F), findall(x, consult(F), L)` gives `L = []` and one line `F: 実行中の述語は置き換えられない`, the key `predicate_running`), a test passes in which the diagnostic is not written twice when the whole query fails, and `check_consult_failure_reports` passes with its expected values unchanged; a program that calls `use_module(library(apply))` as a goal is built with `mosac build` in two forms, with the call in a clause body and in an `initialization/1` goal, and the executable runs where there is no `$MOSA_LIB` and no `lib/` next to it (ctest); in `mosatest --suite tests/conformance` the ISO pass rate does not fall below 98.4% and the library pass rate not below 98.0%; `mosatest --manual docs/manual --all-tables` passes; all CI jobs pass on 3 OSes |
| 13 | Conditional compilation and libraries: the directives `:- if(Goal)`, `:- elif(Goal)`, `:- else` and `:- endif` on every path that reads a text, with `mosac` evaluating the conditions at build time and the executable evaluating them again at start-up (sections 3 and 6, ADR 0027), and `library(pairs)`, `library(assoc)` and `library(ordsets)` (section 5) | The new conditional compilation test file `tests/pl/cond_compile.pl`, `tests/pl/lib_pairs.pl`, `lib_assoc.pl` and `lib_ordsets.pl`, and the new rows of `tests/pl/japanese.pl` pass under `mosa`, and `mosatest --diff` shows zero differences; four programs whose branch differs between build time and start-up are built with `mosac build`, and each executable gives the same output as `mosa` where there is no `$MOSA_LIB` and no `lib/` next to it (ctest): one where only the branch taken at start-up loads a library, one where only the branch taken at build time loads a module file (that module's initialization does not run, and `M:G` gives `existence_error(module, M)`), one where a later term uses an operator that `op/3` declares in a branch not taken, and one where only a branch not taken defines an exported predicate; when the branches are the same, the output of `translate_program` translates only the predicates of the branches taken (ctest `codegen`); in `mosatest --suite tests/conformance` the library column counts the 39 items of `directives/conditional_compilation` and all of them pass, the library pass rate does not fall below 98.0% and the ISO pass rate not below 98.4%, and the ISO denominator stays 2326; `mosatest --manual docs/manual --all-tables` passes; all CI jobs pass on 3 OSes |
| 14 | Debugger: the ports call, exit, redo, fail and exception in `mosa` (ADR 0028); `trace/0`, `notrace/0`, `debug/0`, `nodebug/0`, `spy/1`, `nospy/1`, `nospyall/0`, `leash/1` and `debugging/0` in `system`; the commands of the port prompt; the write option `max_depth/1` | (1) The transcript tests of the ports (`run_session`) pass in Japanese and English: a small recursive predicate followed in creep with all five ports shown, with `Machine::choice_depth()` after a traced deterministic query the same as without tracing; `mosa` with a standard input that is not a terminal, which reads no command and only writes the port lines; an exception leaving a box in `catch/3`; skip, leap with a spy point, fail, retry, abort (back to the prompt), nodebug, `g`, `w`, `+`, `-`, `h` and end of input; a port that `leash/1` does not stop at, which writes its line and reads nothing; a library predicate that is one box, and a user predicate that `maplist/2` calls that is shown; a depth-cut display; predicates and atoms with Japanese names. (2) The tests of (1) pass under `MOSA_GC_STRESS` too. (3) The new rows of `tests/pl/debugger.pl` and `tests/pl/japanese.pl`, and the rows of `max_depth` in `tests/pl/write.pl` (owner decision A on question 1, 2026-11-18), pass under `mosa`, and `mosatest --diff` shows zero differences. (4) A program that calls the debugger predicates is built with `mosac build`, and the executable gives the same standard output as `mosa` where there is no `$MOSA_LIB` and no `lib/` next to it (ctest). (5) The interpreter time of `bench/nrev.pl` and `bench/queens.pl` with the debugger off (Release, the same machine, the median of 3 runs) is within 1.05 times that of 0.4.1; the time with `debug/0` and a spy point on a predicate that neither benchmark calls is also measured, and is within 1.2 times the time with the debugger off (owner decision A on question 2); both are measured with the `mosa` command, and the runs with the debugger on use a `mosatest --bench` file with a directive that turns it on; the `mosac` `nrev` stays 3 times or more faster than the interpreter; the values are written in `bench.md`. (6) In `mosatest --suite tests/conformance` the ISO pass rate does not fall below 98.4% and the library pass rate not below 98.0%, and the ISO denominator stays 2326 (the 14 items of `max_depth_option_supported` stay in their own column). (7) `mosatest --manual docs/manual --all-tables` passes. (8) All CI jobs pass on 3 OSes |

**Prohibitions across phases**

- Do not change the Cell representation decided in Phase 1 or the IR decided in Phase 2 until an ADR (`docs/adr/`) has been written and approved
- Do not add features from later phases "while you are at it". Do not write groundwork for GC or arbitrary-precision integers before Phase 7 either
- Do not leave conformance suite failures unaddressed as "differences in interpreting the spec" without recording them in `docs/iso-notes.md`
