English | [日本語](0015-first-argument-index.ja.md)

Status: Approved (2026-11-02; plan in docs/superpowers/plans/2026-11-02-m3-first-argument-index.md, approved by the owner). Replaces the clause-position sentences of [ADR 0004](0004-clause-database.md) and the dead-clause rules of [ADR 0008](0008-logical-update-view.md)

## Context

A call compares its first argument with every clause and lists every live clause as a candidate, retract and clause/2 unify every clause, asserta shifts the clause vector and every open scan, and each removal erases from the vector and renumbers every open scan, so on N clauses each of these costs O(N) (review 2026-10-31 M3: 20,000 retracts by key 12.9 s, 40,000 assertas 5.6 s, 20,000 `p(_,_), !` 1.1 s).
Every backtrack and clause redo copies the top `Entry` with its candidate list, so one scan over N clauses costs O(N^2).

## Decision

Clauses live in a deque in clause order; each has an id that never changes or repeats (asserta takes one below the first, assertz one above the last) and the database generations at which it was added and removed, and a scan sees exactly the clauses alive at its own generation (the logical update view of 7.5.4). A scan with an unbound first argument keeps a cursor (next id, generation); one with a bound first argument lists candidate ids, taken for predicates of 8 or more clauses from a first-argument index (atom, integer, float with -0.0 = 0.0 and NaN = NaN, list, name/arity; plus keyless clauses) that asserta and assertz extend; retract, retractall and clause/2 use the same index.
A removed clause is erased at once when it is first or last and no open scan of the predicate can still reach it (a scan older than the removal that has not passed it), otherwise in one sweep once the removals since the last sweep exceed half the clauses; ids do not change, so nothing is renumbered. Backtracking and redo read the top `Entry` in place.

## Rejected alternatives

Keeping positions as candidates (ADR 0004/0008) and renumbering on asserta and removal: asserta and middle removals stay O(N) and an unbound call still lists every clause.
Generation numbers without stable ids, or a linked list of clauses: positions still shift on asserta, and a list loses the random access that `enter_clause`, mosac and the native fingerprint use.

## Consequences

Calls, retracts and clause/2 by a bound first argument cost the bucket plus the keyless clauses, an unbound call and asserta cost O(1) amortized, and a scan over N clauses is linear; a redo finds its clause by id (O(1) when nothing was removed from the middle, else a binary search).
`dead` becomes "has a removal generation"; `held_by_scan` becomes a check by generation and id, `note_asserta` and the clause half of `note_removed` go; `live_count` becomes a counter. The index holds keys from clause heads only, no heap cells, so the GC does not see it.
Tests: `index_scale` (with 4 times the clauses: scan, asserta, unbound first call, lookups, retract, retractall, clause/2 and retract from the front take less than 8 times as long) and `tests/pl/database.pl` `m3_*`.
