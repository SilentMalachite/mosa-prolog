English | [日本語](0002-cell-layout.ja.md)

Status: Approved with this design

## Context

Fix the tag number for integers that do not fit in 61 bits before implementation.
Fix the bit layout of functor words here as well.
Also fix here that payloads are not terms.

## Decision

Tags 0 to 7 are Ref, Atom, Int, Str, Lis, Float, Big, IntBox.
Immediate integers are at least -2^60 and less than 2^60; every other int64 is an IntBox.
A functor word has 0 in its low 3 bits, the arity in the next 11 bits, and the atom index in the next 32 bits.

## Rejected alternatives

Do not use Big as the box for int64.
Do not make every word in the heap a valid Cell.

## Consequences

Float and IntBox payloads, and functor words, are not terms.
The phase 7 GC does not scan the heap linearly and treat words as terms.
It follows tagged pointers.
