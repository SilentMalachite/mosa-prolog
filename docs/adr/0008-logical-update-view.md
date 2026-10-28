English | [日本語](0008-logical-update-view.ja.md)

Status: Approved (2026-10-28)

## Context

7.5.4 says a call works on the clauses that were present when it was called, and the owner chose that reading (iso-notes 7.5.4, decision B).
ADR 0004 erases a retracted clause from Predicate::clauses and drops it from open choice points, and abolish clears every open scan of the predicate.
The candidate list of a scan already fixes the clauses at call time, so `assertz/1` is logical already; only removal breaks the view.

## Decision

retract, retractall and abolish mark a clause dead instead of erasing it while an open scan (call, clause/2, retract) still has it among the candidates it has not tried; otherwise they erase it as now.
New calls, clause/2, retract and clause counts skip dead clauses; open scans still try them. abolish with an open scan also marks the predicate abolished, which every new lookup treats as absent.
Dead clauses and an abolished predicate are erased at the next change to that predicate that finds no open scan holding them. The rest of ADR 0004 stays (one vector, asserta shifts indexes, IR before visible).

## Rejected alternatives

Keeping clauses behind shared pointers so each scan holds its own snapshot. The storage that compile and codegen read would change shape.
Generation numbers on every clause and choice point. The candidate list already fixes the set at call time, so the counter adds nothing.
Keeping ADR 0004. The 4 logical update rows stay failing, against the owner's decision.

## Consequences

The retract and abolish sentences of ADR 0004 are replaced by this ADR; its status line says so once this one is approved.
Clause and Predicate each get one flag; every place that counts or lists clauses (number_of_clauses, listing, the empty check in enter_user, the copy to the test machine) uses live clauses only.
`retract_open_later` in `tests/pl/database.pl` changes to `[X = 1, X = 2, X = 3]` in the implementing commit. No code is written before approval.
