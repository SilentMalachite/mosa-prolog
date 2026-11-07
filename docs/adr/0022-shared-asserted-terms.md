English | [日本語](0022-shared-asserted-terms.ja.md)

Status: Approved (2026-11-07; the owner asked for review M30 to be fixed in the same request, which counts as approval). This adds a field to the `Clause` of [ADR 0004](0004-clause-database.md); the IR of [ADR 0003](0003-execution-ir.md) and [ADR 0021](0021-wide-ir-slots.md) does not change.

## Context

asserta/1 and assertz/1 compile a clause with the full copy, which meets a shared subterm once per path, so a term with deep sharing becomes a tree: `dag(40, X), assertz(p(X))` builds 2^40 IR nodes and passes the heap limit without stopping (review M30).
A clause has no heap of its own, so its IR cannot point at the asserted term the way a run-time goal does (M28). The clause environment is made at each call and holds a fresh variable per slot.

## Decision

When asserting, a compound subterm met a second time becomes a shared term if it holds no variable, no NaN and no cycle, has at least 64 nodes as a tree (`kShareWeight`; a smaller one costs more to look up than to copy) and is not the root of a copy (the head, a body goal, the term of a `call/1` leaf, a ball, a catcher) or the first argument of the head, which are always copied: its first other copy moves to `Clause::shared` and every other occurrence becomes a `Var` of a slot past the clause's variables (`SharedTable::base`).
The environment holds the variables only. `materialize` builds a shared term at its first read within one head or one body goal, in the same walk as the term that refers to it, and keeps it in `SharedMemo` until that head or goal is built; a memo marks the entries of a per-machine table with its own number, so making and dropping one allocates nothing, and a copy without shared terms takes a path that never looks at a memo. A call thus allocates nothing for shared terms it does not read, a branch that does not run builds nothing, and no continuation keeps a term it read; each head and each goal builds its own copy, as the full copy did. clause/2 and retract/1 build the whole clause the same way, and clause/2, retract/1 and retractall/1 build the body only after the head matches. A shared term is ground and holds no NaN, so which copies are one cell cannot be seen: unification takes a compound as equal to itself without looking inside, and a NaN unifies with nothing (iso-notes 7.2.2), and a variable could be bound to a NaN after the assert; a subterm that reaches a cycle cuts it with its own anonymous variable in each copy (iso-notes 7.3.3).
The IR that an assert builds counts against the heap limit as in `compile_goal`, so a shape that is still expanded (a shared control skeleton, a repeated subterm with a variable, a NaN or a cycle) stops at the limit; a smaller repeated subterm is copied, so the IR stays within about 64 times the size of the term with its sharing. Copying the clauses into another machine (the Logtalk-suite judgments) compiles a clause with shared terms the same way.

## Rejected alternatives

A new IR tag for a shared node. Every walker of the IR (materialize, clause/2, mosac, the fingerprint) would need it, and a `Var` of a slot already means "this term".
Share the roots and the first head argument too. The root of a goal must stay a goal (a `Var` there runs as `call/1`), and the first argument gives the clause its index key. Copying every argument of the head and of a goal instead would make `p(T, T, …)` cost the number of occurrences times the arity of T.
Share loaded clauses too. They are trees as read, and mosac's generated code reads every slot as a variable. Keep each shared term in an env slot for the whole call: each call then makes a slot per shared term, and the continuation keeps every term it read (Codex reviews of M30).

## Consequences

Asserting a clause, and building its head or one of its goals, cost time and memory in the size of the term with its sharing, not of its tree. Loaded clauses and clauses without such a subterm are compiled exactly as before.
mosac does not generate code for a predicate with shared terms (`native_eligible`); only asserted, so dynamic, clauses have them.
Tests: `m30_*` in tests/pl/database.pl, tests/cpp/assert_shared_test.cpp.
