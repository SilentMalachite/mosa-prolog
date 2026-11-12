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

## Amendment 1 (2026-11-12, phase 10, ADR 0025; approved with ADR 0025 by the owner's decision A on 2026-11-12)

BIG is used as ADR 0025 says (a pointer to a head word and the magnitude words).
`max_integer` and `min_integer` stay the int64 bounds (iso-notes 7.11.1.2, default A).
