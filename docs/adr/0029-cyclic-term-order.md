English | [日本語](0029-cyclic-term-order.ja.md)

Status: Approved (2026-11-20, owner; the owner's request to work on H-04 of the 2026-11-20 review counts as approval). 7.2 orders finite terms only, and 7.3.3 leaves cyclic terms undefined; this ADR fixes the order that `==`, `compare/3`, `@<` and the sorts give them.

## Context

`compare_seen` (`src/builtins/order.cpp`) walks both terms depth first and counts a pair of compound terms that it meets again as equal. The answer then depends on the pair where the walk notices a cycle, so the order is not transitive: with `A = f(A, b)`, `B1 = f(B1, b)`, `B = f(B1, B)`, `C = f(C1, C)`, `C1 = f(C, a)` it gives A < B, B < C and C < A, and `sort/2` gives different lists for different input orders (review H-04). `std::stable_sort` needs a strict weak order, so in C++ this is undefined behaviour.
No total order keeps the argument-by-argument rule of 7.2 for all cyclic terms. With `A = f(A1, b)`, `A1 = f(A, c)`, the first arguments of A and A1 are A1 and A, so the rule makes A against A1 the same as A1 against A, although the two terms differ.

## Decision

A term is ordered by two keys of its unfolded tree, compared in turn. The first key is its nodes in depth-first, left-to-right order, each node compared by the rules of 7.2 for a variable, a number, an atom, or the arity and then the name of a compound term. Only when the two first keys are the same and infinite, the second key decides: the nodes level by level, left to right, so the shallowest difference, then the leftmost, decides.
The walk compares the first key as it does today, and a pair that it meets again after it finished that pair equal is still equal (shared subterms). A pair met again while it is still open means that the two first keys go on equal forever; the walk then compares the second keys of the two given terms, breadth first from the top, and does not queue a pair twice. So `compare(=)` holds exactly when the trees are equal, as `==` and unification say.
Both walks keep their pairs in the table of ADR 0010 amendment 4, which counts the pairs past the heap words in use against the heap limit; the breadth-first walk counts its queue with its pairs. Unification keeps "a pair met again is equal", which is sound for equality.

## Rejected alternatives

Keeping "a pair met again is equal" for the order: it is not transitive (the triple above). Raising an error for cyclic terms, or ordering them by address: `compare/3` and the sorts of cyclic terms, which work today, would stop or would depend on the heap layout.
Only the level-by-level key: it disagrees with 7.2 on finite terms (`f(g(b), a)` against `f(g(a), b)`). Canonical forms by a minimal automaton (the review's proposal): equal trees get equal forms, but an order on the forms that agrees with the depth-first walk wherever that walk decides still needs a tie-break such as the second key, plus a partition refinement in every comparison.
Comparing the trees cut at a growing depth and taking the limit: for some pairs the answer alternates with the depth and has no limit (A and A1 above).

## Consequences

Finite terms, and every pair whose depth-first walk reaches a difference, keep the order of 7.2 and of today. The keys depend only on the unfolded tree, so a term and its unrolled form (`X = f(X)`, `Y = f(f(Y))`) compare the same against every term. Some cyclic pairs change their answer: with `A = f(A, b)`, `B = f(B, c)`, `f(A, z)` now comes after `f(B, a)` (the shallowest difference, z against a), where before it came first (b against c, met after the walk counted the pair (A, B) as equal).
A comparison that falls to the second key walks the pairs once more, in time and table size linear in the pairs that it meets; the depth-first walk keeps the bound of ADR 0011. iso-notes 7.3.3 (en/ja) records the order.
Tests: `tests/pl/order.pl` (the review triple in all six input orders, a generated set of cyclic terms and their unrolled forms), `tests/cpp/pair_limit_test.cpp`.
