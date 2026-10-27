English | [日本語](0001-cell-representation.ja.md)

Status: Approved (2026-10-24)

## Context

The interpreter and `mosac` share the same runtime.
The term representation is the unit of that sharing.
Fix the representation in a document before implementing it in phase 1.

## Decision

A 64-bit tagged integer `Cell`. The low 3 bits are the tag: REF, ATOM, INT (61-bit immediate), STR (pointer to a functor cell in the heap), LIS (list), FLT (boxed double), BIG (reserved for future use). Integers that do not fit in 61 bits are boxed in the same way as FLT, and `max_integer` stays 2^63−1

## Rejected alternatives

The instructions document lists no alternatives for comparison. This ADR does not add any.

## Consequences

Phase 1 implements Cell with this representation.
To change it, write a new ADR and get the project owner's approval.
Phase 0 code does not implement Cell.
