English | [日本語](0003-execution-ir.ja.md)

Status: Approved (2026-10-24). This approval is the approval of the phase 2 design. [ADR 0009](0009-native-code.md) adds the `Ctrl::Native` control frame and the generated functions that run on the same trampoline (2026-10-29)

## Context

The interpreter and mosac execute the same IR.
To avoid changing the representation until phase 6, fix it before implementing execution.
Goal frames are not terms.

## Decision

A clause becomes an IR that holds its continuation last, and is executed on a trampoline.
A goal frame is a non-term on the heap and holds the predicate, arity, cut barrier, continuation and arguments.
Choice points and catch frames are pushed onto one stack, and the trail is kept even after success.

## Rejected alternatives

Do not walk the clause term on every execution.
Do not use a WAM environment stack and instruction sequence.

## Consequences

The phase 7 GC includes goal frames among the marking roots and does not scan them as terms.
To change this representation, write a new ADR and get the project owner's approval.
Implementation details follow docs/superpowers/specs/2026-10-24-phase2-engine-design.md.
