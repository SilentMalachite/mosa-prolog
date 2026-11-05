English | [日本語](0006-modules.ja.md)

Status: Approved (2026-10-28). The sentence on control frames keeping word 1 as the extras index is narrowed by [ADR 0009](0009-native-code.md) to frames whose word 1 has bit 63 clear (2026-10-29). Amendment 1 (2026-11-06, review L19 A1, owner decision A; approved by the owner's choice of A to be implemented): a file loaded through `library(...)` is read with the ISO core in place of the `user` table

## Context

Phase 4 adds the module system of SPEC section 5. Today a predicate is found by (name, arity) only, and a goal frame does not say which module it runs in.
`Module:Goal`, `meta_predicate/1` arguments and an unqualified `assertz/1` all need the context module at run time.
The shape must be fixed before code, because phase 6 `mosac` generates the same goal frames (ADR 0003).

## Decision

Predicate gets a module (the atom number of the module name), and the key becomes (module, name, arity). `system` holds the C++ built-ins and control constructs; `user` is the default module.
In a goal frame (word 0 is not 0), word 1 holds the arity in bits 0–15 and the atom number of the context module in bits 16–47. kFrameHead stays 4 and the IR is unchanged. A body goal gets the module of its clause's predicate; only `M:G` and `meta_predicate/1` arguments switch it.
Lookup tries `system` first (defining a `system` name gives `permission_error(modify, static_procedure, PI)`), then the context module's own definitions and imports, then its default import source. Operators are looked up module-local, then `user`, then the ISO core; `op/3` in an export list goes into the importing module's local layer.

## Rejected alternatives

A fifth head word in every goal frame. Every frame would grow by one word for a value that fits in the arity word.
Resolving the predicate when the clause is loaded and storing its index in the frame. Predicates defined or asserted later would not be found.
One global operator table. SPEC section 5 makes `op/3` local to the module that declares it.

## Consequences

Control frames (word 0 is 0) keep word 1 as the extras index. Every read and write of a goal frame's word 1 goes through `read_goal` and `write_goal`.
Phase 6 `mosac` writes the context module in the same bits. The phase 7 GC does not scan this word as a term.
Reader and Writer take an operator view instead of an OperatorTable. No module code is written until this ADR is approved.

## Amendment 1 (2026-11-06, review L19 A1, owner decision A)

Context: `user` operators are visible in every module, so a library file read after `op(1100, xfy, ->)` (8.14.3) can be read as other clauses with no syntax error; review L19 found `format.pl` read that way, and every message after it fell back to its key.
Decision: while `use_module(library(Name))` loads the file it finds (bundled or on the search path, with the files that file loads), the `user` table is set aside and the ISO core takes its place, so the file is read with the ISO core and its own module's layer; a `user:op/3` made during the load goes into the ISO core and into the set-aside `user` table at once, and is refused if either refuses it, as when the file is loaded by path; afterwards the `user` table comes back with those changes, and a refused load restores it as it was. Nothing changes at run time: a library predicate that reads or writes terms (`~q` of format/2) still sees the `user` operators.
Rejected: B (keep it as a known limit), which leaves library predicates silently wrong after such an op/3; marking library modules so that their view always uses the ISO core, which would make `~q` write the user's terms without the user's operators; reloading the library for messages only, which leaves the program's copy mis-read. Consequences: SPEC section 5's "operators declared in `user` are visible in all modules" does not hold while a library file is read; a library that needs an operator declares it in its module or exports it; a file loaded by path, not through `library(...)`, is read with the `user` table as before.
