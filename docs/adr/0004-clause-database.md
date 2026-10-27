English | [日本語](0004-clause-database.ja.md)

Status: Approved (2026-10-25)

## Context

Phase 2 predicates keep their IR clauses in order in Predicate::clauses.
assert and retract touch that sequence and the indexes of running choice points at the same time.
If the location of clauses is not decided first, each piece of modifying code will decide the shape its own way.

## Decision

Dynamic clauses go in the same clauses. asserta adds to the front, assertz to the end. When inserting at the front, shift by one the choice point indexes at or after the insertion position.
retract removes the clause from later candidates and keeps the remaining goals of the clause now executing. An added clause is converted to IR before it is made visible.
abolish deletes the predicate. The same indicator can be defined statically again by a later consult. Open items 5 and 6 are not decided here.

## Rejected alternatives

Keeping only dynamic clauses in a separate vector. Choice point indexes would point into two sequences.
Having retract also remove the remaining goals of the current clause. The running continuation would vanish midway.
Delaying IR conversion until the first call. A choice point would hold a clause not yet compiled to executable form.

## Consequences

While this status line is unapproved, do not write code that modifies clauses.
Add a dynamic flag to Predicate. The shape of Clause stays as in phase 2.
Open items 5 and 6 remain as questions in docs/iso-notes.md.
