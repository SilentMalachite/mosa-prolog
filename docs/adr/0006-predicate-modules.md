English | [日本語](0006-predicate-modules.ja.md)

Status: Approved (2026-10-27). This approval is the approval of the phase 4 module design.

## Context

Phase 4 lets two modules define the same name and arity. Lookup is one vector keyed by name and arity.
The calling module has to come back on backtracking. It must not change the Cell or the phase 2 goal frame.

## Decision

Predicate identity is (module, name, arity). Predicate.home is the module atom. Built-ins stay in the dispatch table and are the system module.
The calling module is Impl.calling. A choice point records it for redo, and a control frame restores it when a clause succeeds.
Lookup tries the caller's namespace, then that module's default source, then system. user defaults to system. Any other module defaults to user.

## Rejected alternatives

A single table keyed only by name and arity. Two modules could not both define parent/2.
Storing the calling module in a Cell or in the phase 2 goal frame. That would change ADR 0001 or ADR 0003.
Resolving imports to direct calls inside the interpreter. That resolution belongs to phase 6.

## Consequences

Search and insert take a module. A failed load rolls the module record back with the clauses.
The translator still resolves imports in phase 6. A new ADR is required to change this identity.
