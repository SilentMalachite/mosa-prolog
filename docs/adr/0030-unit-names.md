English | [日本語](0030-unit-names.ja.md)

Status: Approved (2026-11-21, owner; the owner's request to fix H-27 of the 2026-11-20 review after writing this ADR counts as approval). For program units it replaces the namespace `mosa_gen::m<mangled module name>` of SPEC section 5 and ADR 0009; owner decision B (2026-10-29: `mangle` never makes `__`) stays.

## Context

A unit of `mosac emit`, `build` and `run` took its namespace `mosa_gen::m<mangle(module)>` and its files `m<mangle(module)>.cpp` and `.hpp` from its module (review H-27). So `utable` gives the keyword `mutable`. `ain` gives `main.cpp`, which the entry file then overwrites, and `'AIN'` gives `mAIN.cpp`, which is `main.cpp` on APFS and NTFS. `foo` and `'Foo'` give two names for one file there. Each non-ASCII byte becomes three characters, so a module name of 28 kanji (84 bytes) gives a file name of 257 bytes, over the limit of 255.
`mosac emit --units` named a unit `u<N>_m<mangle(module)>`, with N the number of the test file, and so has the case and length problems. A prebuilt library unit is compiled with libmosa before any program exists, and is linked only when the program read the same bytes as `lib/<name>.pl` beside `mosac` (`shipped_identical`), so its name can depend only on that library.

## Decision

A program unit is named by its place, never by its module. The unit at position k of `plan_units` (0 is the root file's module, then the other modules in the order of the module table) has the namespace `mosa_gen::u<k>` and the files `u<k>.cpp` and `u<k>.hpp`. In `mosac emit --units`, the unit of the file at position i among the program files that test file N loaded (0 is the test file) has `mosa_gen::u<N>_<i>` and the files `u<N>_<i>.cpp` and `.hpp`. The module name stays in the banner comment of both files, in a comment on the unit's `#include` line in `main.cpp`, and in `NativeUnit::name` as before.
A prebuilt library unit keeps the namespace `mosa_gen::m<mangle(module)>` and the file `<name>.cpp`: only the modules of `lib/` reach it, and a name there that makes a bad identifier stops the build of libmosa, not a user's build. `mangle` also stays for the predicate functions `p<mangle(name)>_<arity>`, which the `_<arity>` keeps apart from every keyword.

## Rejected alternatives

Escaping only the names that break: it takes one rule per kind (a keyword list that grows with the standard and with the identifiers of special meaning such as `module` and `import`, the entry file in any case, and a suffix for names that fold equal, which depends on the program's other modules), and the length stays unbounded.
Keeping the module in the name, as `u<k>_m<mangle(module)>`: it fixes the keyword, the entry file and the case twins, but not the length, and the banner already names the module. A hash of the module name: bounded, but it can collide, and it is no easier to read than the position.
Numbering the prebuilt library units too: they are compiled before any program, so they have no place in its plan.

## Consequences

Every module name gives a valid namespace, and file names that are a `u` with one or two numbers, never `main.cpp`, and distinct on file systems that ignore case. The prefix `u` keeps a program unit apart from the prebuilt unit of a library module of the same name, such as a user's module `lists` once H-28 translates it. A unit's name depends on the modules loaded before it, and `muser.cpp` of a program without modules is now `u0.cpp`.
SPEC sections 5 and 6 (English and Japanese) and a later line of ADR 0009 follow this ADR. Tests: `tests/cpp/unit_names_test.cpp` (the names of `Project::files` from `translate_program` and `translate_units` are distinct after ASCII case folding, none but the entry folds to `main.cpp`, and each unit's namespace is its position), and the case `module_names` of `tests/mosac/build_test.cmake` (`mosac run` of a program with the modules `utable`, `ain`, `foo`, `'Foo'` and a 30-kanji name prints what `mosa` prints, on the CMake and the direct paths).
