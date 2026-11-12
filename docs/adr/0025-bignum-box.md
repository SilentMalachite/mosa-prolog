English | [日本語](0025-bignum-box.ja.md)

Status: Proposed (2026-11-12; phase 10 opened by the owner, decision A; waits for the owner's approval before any code). SPEC section 9 gives phase 10; this ADR fixes the heap shape of an integer outside int64 and how the IR, the index and `mosac` carry it.

## Context

A Cell holds an int64 as INT (61-bit immediate) or IntBox, and tag 6 (BIG) is reserved (ADR 0001, 0002). Phase 10 adds integers of any size, so the shape on the heap, in the IR, in the clause index and in generated C++ must be fixed before code depends on it.
The GC (ADR 0010) and the copy paths (assert, findall, `sol::CopyKind`, `ExportedTerm`) all walk terms, so a shape they cannot size from its first word would need a new rule in each.
A value that fits int64 must stay an INT or IntBox, so programs that use only int64 take the same path and cost as today.

## Decision

Box: a Cell with tag 6 points at a head word `(n << 4) | (negative << 3)` (low 3 bits 0), followed by n 64-bit words of the absolute value, least significant first, the last word not 0; the value is always outside int64. The GC marks and moves n+1 words as Raw.
IR: add `IrTag::Big`; `integer` is 1 if negative, 0 if not; `args` holds the magnitude words from the least significant, as `IrTag::Int` children (`integer` = `std::bit_cast<std::int64_t>(word)`); `ExportedTerm::Kind::Big` and `sol::CopyKind::Big` carry it the same way (child nodes of the Int kind). Index: add `KeyKind::Big` with the key `big_key(negative, magnitude)` (FNV-1a) in the `Clause::key_int` field. `mosac` emits a constant as `static constexpr std::array<std::uint64_t, N>` and `rt::put_big`.

## Rejected alternatives

A value field in `IrTerm`: every node of every clause grows for a rare case.
A constant table per clause: a goal translated at run time has no clause to hold it.
Two's complement words: product and quotient are harder to write than with a sign and a magnitude.

## Consequences

`max_integer` and `min_integer` stay the int64 bounds (ADR 0001 gets one line; iso-notes asks "A or B?"); ADR 0002 gets one line that a Big box has a head word and is not scanned as terms.
Tasks 3-8 of the phase 10 plan implement it (consumers first, producers after); `bounded` becomes `false` with the arithmetic in task 6; ADR 0001's "BIG reserved" becomes "BIG in use".
Tests: `tests/pl/bigint.pl`, C++ tests of the box, copy and index, `MOSA_GC_STRESS` runs, and `mosatest --diff`.
