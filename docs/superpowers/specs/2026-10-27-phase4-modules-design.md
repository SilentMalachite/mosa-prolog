# Phase 4 modules

Date: 2026-10-27
Status: draft, waiting for approval

English is the canonical text. The Japanese translation is `2026-10-27-phase4-modules-design.ja.md`.

## Intent

Phase 4 adds the module system of `docs/SPEC.md` section 5 to the interpreter. The reader is the agent who implements one session at a time, and the owner who approves ADR 0006 before the lookup code changes.

The picture of done is one terminal session. `consult` loads a module file and leaves the toplevel namespace untouched. `use_module` brings the exported predicates in. A goal inside the module calls `maplist` and an imported alias, and those calls reach the module that wrote them. `Module:Goal` runs in the named module. A predicate that was not imported is invisible under its bare name. Importing the same predicate from two modules aborts that load.

## Scale

This document is the design. Implementation stays inside phase 4. The four stages below stay in this order. Each bullet inside a stage is one session, because one session takes one predicate, one file, or one bug. The version string stays `mosa 0.0.1 (phase 3)` until the closing session.

## Goals

- The morning session in this document succeeds, byte for byte in the answers it names
- The declaration example in `docs/SPEC.md` section 5 loads, and `家系:祖父(太郎, Z)` answers `Z = 三郎` once the two parent facts exist
- `tests/pl/modules.pl` passes under the existing `prolog_tests` ctest
- The first six libraries exist as Prolog files: `lists`, `apply`, `between`, `format`, `strings`, `japanese`
- After the closing session, `cmake --build build && ctest --test-dir build` passes and `mosa --version` prints `mosa 0.0.1 (phase 4)`

## Non-goals

- `mosac` module mangling, C++ namespaces, and static import resolution. That is phase 6
- GC, `garbage_collect/0`, `statistics/2`
- `nb_setval/2`, `nb_getval/2`, `setup_call_cleanup/3`, `forall/2`, `sort/4`, `keysort/2`, `tab/1,2`, `term_to_atom/2`, `number_vars/3`
- Full-width symbol mode and Japanese diagnostics. Phase 5 owns that behavior. Phase 4 only saves and restores the two flags named below
- New functions on `include/mosa/mosa.hpp`
- Editing the Logtalk suite, its exclusion list, or the phase 3 denominator

## Anti-goals

- A session that leaves the existing ctest red. The first behavior change is the `user` / `system` split, and the suite must stay green across it
- A failed import that leaves clauses, exports, or a `current_module/1` answer behind
- A meta-argument wrapper that changes the answers of today's `call`, `findall`, `bagof`, `setof`, or `catch` tests
- Implementing a non-goal because a library predicate would be easier with it
- Weakening a test, skipping it, or rewriting an expected value to match the implementation

## Constraints

- C++20 and CMake only. No new libraries
- Warnings are errors. A function is 60 lines or fewer. A file is 800 lines or fewer
- Identifiers are English. Comments and commit messages are Japanese. Commit messages start with `[phase4] `
- Prolog failure and `throw/1` are machine state. The only C++ exception is `ResourceLimit`
- Errors go through `throw_error` and `make_type_error`. Atom names are not string literals at the throw site
- Tests compare the Formal term only
- Cell and IR stay as ADR 0001, 0002, and 0003. Predicate lookup changes only after ADR 0006 is written
- OS-specific code stays in `src/platform/`
- A new ISO uncertainty is one question of the form "A or B?" in `docs/iso-notes.md`, with the Japanese twin updated, and implementation of that point waits
- Header comment of a built-in file: ISO clause when it has one, otherwise `docs/SPEC.md` section 5, plus argument modes, errors, and the test file name

## The morning session

Two files, then this toplevel. Answers are the lines without a prompt.

`人名.pl`

```prolog
:- module(人名, [姓名分解/3, 読み/2]).

姓名分解(太郎, 太, 郎).
読み(太郎, たろう).
```

`家系.pl`

```prolog
:- module(家系, [親/2, 祖父/2, 確認/0]).
:- use_module(library(apply)).
:- use_module(人名, [姓名分解/3 as 分解]).

親(太郎, 次郎).
親(太郎, 花子).
親(次郎, 三郎).
祖父(X, Z) :- 親(X, Y), 親(Y, Z).
確認 :- maplist(親(太郎), [次郎, 花子]), 分解(太郎, _, 郎).
```

```text
?- consult('人名.pl'), consult('家系.pl').
true.
?- use_module(家系).
true.
?- 祖父(太郎, Z).
Z = 三郎.
?- 確認.
true.
?- use_module(library(lists)).
true.
?- member(花子, [次郎, 花子]).
true.
?- 家系:祖父(太郎, Z).
Z = 三郎.
?- 読み(太郎, Y).
false.
?- 人名:読み(太郎, Y).
Y = たろう.
```

A third module that imports `親/2` from two modules fails while loading. The Formal term is `permission_error(import_into, procedure, 親/2)`. That module's clauses are absent afterwards, and `current_module/1` does not answer that module.

`読み/2` is exported by `人名`. The bare call fails because `user` did not import it.

## Decisions from the conversation

- `consult` of a module file does not import into the caller. `use_module` does
- A second `consult` of the same file replaces that module's clauses
- `use_module` of a module that is already loaded does not append those clauses again
- Importing one local name/arity from two places aborts the load that caused it
- `Module:Goal` may call a predicate that is absent from the export list. The export list decides what `use_module` pulls in
- A meta wrapper is applied once. When the caller is `user`, the goal reaches the same predicate an unqualified goal reaches today
- A file with no `module/2` is `user`. Built-ins and control constructs are `system`. `user` sees `system` by default
- `predicate_property/2` reports the defining module, whether the predicate is exported, where it was imported from, its `meta_predicate` spec, `dynamic`, and `multifile`
- Operators local to a module, flag save/restore, `multifile`, and the four libraries besides `lists` and `apply` are in this phase. The morning session is green before that remainder starts
- The version string changes in the closing session

## Session order

Stage 1. The suite stays green.

1. Write ADR 0006 and its Japanese twin. No behavior change in that session
2. Give every predicate a module. Built-ins live in `system`. Clauses from files live in `user`. `user` sees `system`. Existing tests pass. No new syntax

Stage 2. `祖父` answers.

3. `module/2` and `current_module/1`
4. `use_module/1`
5. `use_module/2`: an import list, `PI as Alias`, `except/1`, and the conflict error
6. Library search path, `library(lists)`, and `length/2` in `system` re-exported by that library. `member/2` then succeeds

Stage 3. `確認` answers. The morning session is green after this stage.

7. `Module:Goal` and `existence_error(module, Module)`
8. `meta_predicate/1`, and specs on the built-ins named under Behavior. Existing call and all-solutions tests stay green
9. `library(apply)`, including `maplist/2` through `maplist/5`

Stage 4. The rest of section 5, then close.

10. `asserta/1`, `assertz/1`, `retract/1`, `retractall/1`, `abolish/1`, `clause/2`, and `dynamic/1` honor the calling module and a qualified head
11. `multifile/1` of `Module:PI`
12. `op/3` local to the declaring module, operators in an export list, and `user` operators visible in every module
13. Save and restore `double_quotes` and `japanese_punctuation` around each file load
14. `library(between)`
15. `library(format)`
16. `library(strings)`
17. `library(japanese)`
18. `export/1` and `predicate_property/2`
19. Cover the section 5 declaration example in `tests/pl/modules.pl`, keep the whole suite green, set the version string to `mosa 0.0.1 (phase 4)`, and update `docs/STATUS.md`

Write the tests for a predicate before implementing that predicate. Fixture programs live in `tests/pl/modules/` and are consulted the way `tests/pl/operators.pl` already consults a fixture.

## Behavior

### Where a predicate lives

ADR 0006 records this decision before session 2 changes code.

Predicate identity is `(module, name, arity)`. Two modules may define the same name and arity. The calling module is engine state. It is not stored in a Cell. An import entry sits on the importing module and maps a local name and arity to a source module and source name.

Rejected in that ADR: a single global predicate table keyed only by name and arity; storing the calling module in the Cell; resolving imports to direct calls inside the interpreter. The last of those belongs to phase 6.

Lookup from the calling module, first hit wins:

1. That module's own definitions. Local predicates and imports share one namespace
2. Its default import source. For an ordinary module this is `user`. For `user` it is `system`
3. `system`

Step 2 sees the default source's own namespace, including what that source imported. Imports are not re-exported to a third module by `use_module`. A module sees a `user` predicate through step 2 because section 5 says the default source is `user`, and then sees built-ins through step 3 even when `user` has no import of its own to offer.

Defining a built-in name in a user module raises `permission_error(modify, static_procedure, PI)` at load time. The same Formal is used when a module body tries to add clauses to a `system` predicate.

The calling module is the module of the clause that is running, or the module named by `Module:Goal`. Lookup uses it. Meta wrapping uses it. Entering the callee then sets the calling module to the callee's home module before the callee's body runs. Backtracking restores the caller's module.

Call boundary for a meta predicate:

1. Resolve the predicate in the current calling module
2. Wrap the meta arguments with that same calling module
3. Enter the callee and set the calling module to the callee's home
4. Run the body

`library(apply):maplist` therefore receives `家系:親(太郎)` when `確認` calls it, and the body of `maplist` runs with `apply` as the calling module.

### Loading

`module/2` is the first term of a module file. The exports are a list of predicate indicators and `op(Precedence, Type, Name)` terms. A later `export/1` adds indicators. `export/1` affects later imports. A module that already imported this one keeps the export list it saw.

A file whose first term is `module/2` loads its clauses into that module. `consult` does not import the exports into the caller. A file with any other first term loads into the calling module, which is `user` at the toplevel.

A second `module/2`, or a `module/2` that is not the first term, fails the load with `permission_error(modify, module, Module)`. None of that file's clauses, exports, or operators remain. `module/1` is not a directive this phase adds.

A different file whose `module/2` names a module that already has a source file fails with the same Formal. The existing module stays. The new file leaves nothing. Section 5 says one file corresponds to one module. Clauses from another file arrive only through `multifile`, and that file names its own module.

Loading one module file is atomic for that file, including when `initialization/1` fails or throws. On failure, its clauses, imports, exports, operators, and `current_module/1` entry are gone. A dependency that this file loaded successfully stays loaded. A file that is not a module file keeps the `consult` behavior it has today.

`consult` of a file that already defines that module replaces the clauses that came from that file when the load succeeds. Clauses that other files added under `multifile` stay. Imports in the file are processed again. If that reload fails, the module returns to the clauses, exports, operators, and imports it had before the attempt. The failed attempt adds nothing and removes nothing that was already there.

`use_module(Spec)` loads `Spec` when the target is not loaded, then imports every export into the caller. `use_module(Spec, [PI, ...])` imports only those indicators. `use_module(Spec, except([PI, ...]))` imports every export except those indicators. `PI as Alias` imports `PI` under the local name `Alias`. The source predicate keeps its name. `as` is recognized inside this import list. It is not added to the global operator table, so an ordinary term `a as b` reads as it does today.

`Spec` is resolved in this order:

1. `library(Name)` searches `$MOSA_LIB` when that variable is set, then `lib/` beside the executable, for `Name.pl`
2. An atom that is already a loaded module names that module
3. Any other atom is a file spec, relative to the file being loaded, or to the process working directory at the toplevel. Try the atom as a path, then the atom plus `.pl`

A loaded module name wins over a file of the same spelling. A missing file fails the way `consult` of a missing file already fails. A `library(Name)` file starts with `:- module(Name, [...]).`

A load that is already in progress for the same file is not re-entered. `module/2` registers the exports before the clauses, so a cycle can import those exports. The load still terminates.

### Qualification and meta

`Module:Goal` requires `Module` to be a ground atom naming a loaded module, including `user` and `system`. A variable raises `instantiation_error`. A non-atom raises `type_error(atom, Module)`. An unknown module raises `existence_error(module, Module)`. The goal then runs with `Module` as the calling module, and lookup sees predicates that are absent from the export list.

In `M1:M2:Goal`, `M2` is the calling module.

`meta_predicate` specs use `:`, `0` through `9`, `^`, and `//`. Wrapping happens once, at the call boundary above. An argument that is already a `:/2` term is left as it is.

- `:` and `0` through `9` wrap the argument as `Caller:Arg`. The digit records how many arguments the callee will append. The engine does the wrapping. The callee appends, usually by calling `call/N`
- `^` wraps the goal of a `Vars^Goal` term and leaves `Vars` in place. Any other term is wrapped whole
- `//` wraps a `Name/Arity` term as `Caller:Name/Arity`

Built-ins that receive this spec in session 8: `call/1` through `call/8` (`call(0)` through `call(7, ?, ...)`), `findall/3` and `findall/4` (`findall(?, 0, ?)` and with the tail), `bagof/3` and `setof/3` (`?(^)` on the goal argument), `catch/3` (`catch(0, ?, 0)`), `once/1` (`once(0)`), and `\+/1` (`\+(0)`). Control constructs compiled into IR (`,/2`, `;/2`, `->/2`, `*->/2`, `!/0`) stay as they are. Their bodies already run under the clause's home module.

`findall` and the other C++ all-solutions predicates execute the wrapped goal through that same call boundary. A `findall` written in `家系` therefore collects answers of `家系`'s predicate.

### Database, multifile, operators, flags

The calling module's namespace slot for a predicate indicator holds either an import or local clauses, not both. Unqualified `assertz/1`, `asserta/1`, `retract/1`, `retractall/1`, `abolish/1`, `clause/2`, and `dynamic/1` apply to local clauses of the calling module. When that slot is only an import, those operations raise `permission_error(modify, static_procedure, PI)` and change neither the import nor the exporter. The qualified form `assertz(家系:親(a, b))` reads or writes the named module's local predicate. An import does not make an assert in the importer add a clause to the exporter.

`multifile(PI)` declares the calling module's predicate. `multifile(Module:PI)` declares that module's predicate. A contributing file has its own module name, declares `multifile(Owner:PI)`, and writes clauses whose heads are `Owner:Head`. A bare clause belongs to the file's own module. Both sides declare the predicate before a foreign clause is legal. A qualified clause without both declarations fails that load with `permission_error(modify, static_procedure, PI)`. A later `consult` of one contributing or owning file removes the clauses that came from that file and appends the newly loaded ones at the end. Clauses from other files stay in their relative order.

`op/3` affects the module that executes it. While a file is being read, the operator table is the system operators, plus `user` operators, plus operators this module declared, plus operators imported from export lists. `user` operators are visible in every module. An exported `op/3` term becomes visible to a module that imports that export.

At the start of each file load, save `double_quotes` and `japanese_punctuation`. Restore both when that load ends, including when it fails. Nested loads save and restore their own pair. The running module does not carry a private copy of either flag after loading. This phase does not change what `japanese_punctuation` does to reading.

### Libraries

Each library is a Prolog file under `lib/`, found through the search above. Predicates that need C++ live in `system` and are re-exported. `include/mosa/mosa.hpp` gains nothing. `length/2` is one of those system predicates: it is ISO core, and `library(lists)` re-exports it. The generated Unicode tables are read only from C++, so the three `library(japanese)` predicates and `upcase_atom/2` are implemented in `system` and re-exported. Extend `tools/gen_unicode_tables.py` and regenerate `src/unicode_tables.inc` when the simple uppercase mapping or the fullwidth compatibility mapping is missing. Do not edit the generated file by hand.

`読み幅/2` uses the East Asian width spans the tables already store. A scalar in a W or F span has width 2. Every other scalar has width 1.

`ひらがな_カタカナ/2` maps U+3041 through U+3096 to U+30A1 through U+30F6 by adding `0x60`, and maps the other way by subtracting `0x60`. A scalar outside those ranges is unchanged. Either direction succeeds when the other argument is unbound and the bound argument is an atom. Both ground and unequal fails.

`全角_半角/2` maps ASCII space with U+3000, maps U+0021 through U+007E with U+FF01 through U+FF5E, and maps the halfwidth katakana block by the Unicode compatibility decomposition of that block. A scalar with no partner is unchanged.

`split_atom/4` follows the shape of `split_string/4`: `split_atom(+Atom, +Separators, +Pad, -Parts)`. `Separators` and `Pad` are atoms of characters. Padding characters are dropped. The parts are atoms.

`atomic_list_concat/2` concatenates a ground list of atomics into an atom. `atomic_list_concat/3` does the same with a ground separator atom, and also splits a ground atom into a list when the separator is ground and the list is unbound.

`upcase_atom/2` uses Unicode simple uppercase mapping, one scalar to one scalar. A scalar with no simple uppercase mapping stays as it is.

`msort/2` is a stable sort in the standard order of terms and keeps duplicates. The other `library(lists)` predicates are the ones named in section 5: `append/3`, `member/2`, `memberchk/2`, `nth0/3`, `nth1/3`, `last/2`, `reverse/2`, `sum_list/2`, `max_list/2`, `list_to_set/2`.

`library(apply)` exports `maplist/2` through `maplist/5`, `foldl/4` through `foldl/6`, `include/3`, and `exclude/3`, with `meta_predicate` specs whose digits match the extra arguments.

`library(between)` exports `between/3`, `succ/2`, and `plus/3`.

`library(format)` exports `format/1`, `format/2`, and `format/3` and accepts `~w`, `~q`, `~a`, `~d`, `~f`, `~e`, `~s`, `~c`, `~n`, `~t`, and `~|`.

Edge cases of the list utilities that this document does not name are the builder's, provided each exported predicate has a test that matches the predicate's usual meaning and the morning session stays intact.

### predicate_property/2

`predicate_property(Goal, Property)` resolves `Goal` from the calling module unless `Goal` is qualified. It succeeds once for each of these that apply:

- `defined_in(Module)`
- `exported`
- `imported_from(Module)`, when the visible predicate was imported
- `meta_predicate(Head)` with the declared head
- `dynamic`
- `multifile`

No other property is required for the phase exit.

## Calls made in this document

These were not separate answers in the conversation. They are fixed here so a builder does not have to guess.

- Predicate identity is `(module, name, arity)`. The calling module is engine state. ADR 0006 says so before the code
- Stage bullets are sessions. The four stages stay in the approved order
- `module/2` that is not the first term, and a second `module/2`, raise `permission_error(modify, module, Module)` and leave nothing from that file
- `as` is special to the import list and is not a global operator
- Library search order is `$MOSA_LIB`, then `lib/` beside the executable. A loaded module name beats a file of the same spelling
- A cycle does not re-enter a file already being loaded. Exports from `module/2` are visible to that cycle
- In `M1:M2:Goal`, the calling module is `M2`
- The digit in a meta spec does not make the engine append arguments. Wrapping is the engine's job. Appending is the callee's
- `export/1` does not update modules that have already imported this one
- Flag save/restore wraps the load only
- `読み幅/2` treats only the existing W and F spans as width 2
- The kana map is the `0x60` block offset. Fullwidth and halfwidth use the ASCII block offset and the halfwidth katakana compatibility decomposition
- The `predicate_property/2` functors are the six listed above
- The Logtalk exclusion list and the phase 3 denominator stay as they are
- A failed module-file load, including a failed `initialization/1`, rolls that file back. A file that is not a module file keeps today's `consult` behavior. A failed reload restores the module that was already loaded
- A second source file that repeats `module(Same, ...)` raises `permission_error(modify, module, Same)` and leaves the first file's module in place
- A namespace slot holds an import or local clauses, not both. Unqualified database operations on an import raise `permission_error(modify, static_procedure, PI)` and do not write the exporter
- `multifile(PI)` refers to the calling module. Foreign clauses are written as `Owner:Head` after both sides have declared `multifile`. Reloading one file appends that file's new clauses at the end

## Left to the builder

- How to split `src/module/` and the engine files, within the line limits
- How to store and restore the calling module, as long as backtracking restores it
- How to snapshot a load, as long as the failure and reload observables hold
- The Prolog text inside each library, and the edge cases this document does not name
- Whether `length/2` is C++ or Prolog, as long as it lives in `system` and `library(lists)` re-exports it
- Whether `format` needs a C++ helper for the numeric sequences. A helper lives in `system` and is reached only through `library(format)`
