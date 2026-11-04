English | [日本語](0017-failed-load-keeps-asserted.ja.md)

Status: Approved (2026-11-04; the owner chose A for review M24 and asked for it to be implemented in the same request). Amendment (2026-11-04, approved by the owner choosing B): a failed load keeps the goals' clauses on a predicate its text only declared

## Context

A load that fails puts back the snapshots of the predicates its clauses and declarations touched and erases every predicate added since it began (`rollback_preds`, M18); directive goals take no snapshot, so their changes to existing predicates stay, but a predicate a goal made with assertz was erased, so `:- abolish(q/1). :- assertz(q(7)).` before the failure left no q/1 (review M24).
Snapshots were matched by name only, so one taken before a goal's abolish was put onto the predicate the goal made again; a nested `[F]` or `consult(F)` directive snapshotted every predicate, which also undid what later goals did to them, and the goal form `consult(F), true` snapshotted none, so a failed parent kept the clauses its child's text added (Codex and adversarial reviews of M24).
consult/1 is not ISO; the owner chose A, keeping what the directive goals did, over B, undoing the whole load.

## Decision

A predicate's current definition carries `defined_at` (a value of `Impl::pred_serial`, so it orders with frame creation) and `defined_by_goal`; a definition begins when a frame is made (by asserta/assertz: a goal; by a clause or a declaration: the text), when asserta/assertz or the text reuses an abolished frame, and when abolish under an open scan keeps the frame (a goal).
A snapshot records its frame and definition; a failed load puts a snapshot back only onto the same definition, puts a definition the text began back to the last snapshot of its frame taken of a definition the text did not begin in this load, or erases the frame if there is none, keeps definitions goals began but gives them back the module's pending `meta_predicate` declarations as they were before the load, and erases the predicates of a module the load declared (that module goes back first).
The text takes a snapshot when it first touches a definition it did not begin; a nested load that succeeds hands its snapshots to its parent after its initialization goals, so with those of the loads the goals ran, instead of the parent snapshotting every predicate before `[F]` or `consult(F)`.

## Rejected alternatives

B, undoing the whole load: a goal can write output, open streams or finish a nested consult, which cannot be undone, and a snapshot at every assert, retract and abolish would put the load's state into the database built-ins and put erased predicates back into the table in serial order (M18).
Matching snapshots by frame (`serial`) alone: abolish under an open scan keeps the frame, so the answer would depend on whether a scan was open (adversarial review A3).
Recording who added or removed each clause: undoing the text's removals would bring dead clauses back to life, which the skips of ADR 0015 rule out.

## Consequences

After a failed load, the goals' assertz, retract and abolish stay whether or not a scan was open, and the clauses the text added go, those of nested loads too; a definition the text touched goes back to how it was before the text first touched it, so goal changes made to it after that go as well (`:- dynamic(q/1). q(8). :- assertz(q(7)).` leaves q's old clauses), and a directive's `op/3` and flag changes are still undone while its asserts are not.
A list `[a, b]` whose later file fails no longer puts back every predicate's clauses by name (`restore_clauses` is gone; it put live clauses back into a frame a goal had abolished, adversarial review A1): b undoes its own text, and the load that ran the list, which fails with it, undoes a's text from the snapshots a handed up; a predicate a child's goal made that took over a `meta_predicate` declaration the parent left pending keeps the parent's mark, so the parent's next clause keeps the declaration (A2); `Predicate` grows by 8 bytes to 296 and `Snapshot` by 16 to 112 with libc++, which alone made the consult of 20,000 one-clause predicates (it scans the table for each clause) go from 0.45 s to 0.60 s on an Apple M1 Max; `remember_pred` now takes the predicate its caller looked up instead of a second scan and searches the snapshots from the back, so that consult takes 0.39 s and 2,000 predicates of 25 clauses 0.07 s instead of 0.11 s.
Tests: `tests/pl/failed_load.pl` (52 tests, 18 of which fail on 1e82da5); a load that adds clauses to an existing predicate runs without a choice point (under `\+` or findall it stops at the first such clause), and a load that ends in `:- fail.` records first that it got there.

## Amendment (2026-11-04, review M24 second adversarial review A3; approved by the owner choosing B and asking for it to be implemented)

Context: a declaration counted as text, so before a failure `:- dynamic(q/1). :- assertz(q(7)).` put q back to its old clauses and erased a new q, while the same assertz without the declaration stayed; a declaration changes no clause, and the owner chose B over A, keeping the goals' clauses on a predicate the text only declared.
Decision: `Predicate::text_clause_at` holds the `db_generation` of the last clause the text added to the current definition (0 when a definition begins) and a snapshot holds the `db_generation` at which it was taken; a failed load restores a snapshot in full only when the text added a clause after it, else it puts back only `meta` and, when no live clause is left, `dynamic`; a definition a declaration began that got no clause from the text but has live clauses becomes a definition of the goals (dynamic, with the module's pending `meta_predicate` declarations as before the load), otherwise it goes as before.
Rejected: a flag on the predicate, since a nested load cannot tell its parent's text clauses from its own; a mark on each clause, since a sweep can erase a dead text clause, so the answer would depend on when sweeps ran.
Consequences: `:- dynamic(q/1). :- assertz(q(7)).` leaves `[1,7]` and a new r declared that way keeps `[7]`; a declaration alone still goes, and a predicate the text added a clause to still goes back to before the text first touched it, goal changes included; `Predicate` grows by 8 more bytes to 304 with libc++ and `Snapshot` by 16 to 128, and the consult of 20,000 one-clause predicates takes 0.30 s and that of 2,000 predicates of 25 clauses 0.06 s (0.47 s and 0.11 s on 1e82da5). Tests: `m24b_*` in `tests/pl/failed_load.pl`.
