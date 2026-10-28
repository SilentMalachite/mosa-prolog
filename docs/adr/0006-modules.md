English | [日本語](0006-modules.ja.md)

Status: Approved (2026-10-28). The sentence on control frames keeping word 1 as the extras index is narrowed by [ADR 0009](0009-native-code.md) to frames whose word 1 has bit 63 clear (2026-10-29)

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
