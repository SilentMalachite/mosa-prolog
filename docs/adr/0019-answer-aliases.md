English | [日本語](0019-answer-aliases.ja.md)

Status: Approved (2026-11-04; the owner chose A for review M26 and asked for it to be implemented, which counts as approval)

## Context

`format_answer` leaves out a binding whose value is an unbound variable that an earlier binding showed, and the writer writes a named variable by its name without following its binding (review M26).
So a name can vanish (`X = Y, Y = Z.` answers `X = Z.`; `findall(f(X)-X, true, [Y-P]).` never shows P), and one variable can get two texts (`findall(X-X, true, [P-Q]), Y = g(Q).` answers `P = _G1, Y = g(Q)`).
The owner chose A, showing the aliases, over B, writing a made-up name for each of them.

## Decision

The names whose values are the same unbound variable form a class, in binding order. A class of two or more names N1 … Nk is shown as the chain `N1 = N2, N2 = N3, …` at the places of N1 … Nk-1, whatever the earlier bindings showed; a variable that is no query variable's own is written as Nk inside the other values.
A class of one name whose variable is that query variable's own is shown as before, `N = N` unless an earlier binding showed it; any other class of one name is always shown (`P = _G1`), since a made-up name does not tell which name its variable has.
Bound values are written as before; bindings with equal values are not grouped (`X = f(A), Y = f(A)`).

## Rejected alternatives

B, a made-up name for every name of the variable (`X = _G1, Y = _G1, Z = _G1`): it hides the names the user gave, and the writer would have to stop writing a named variable by its name.
Grouping bound values too (`X = Y, Y = f(A)`): every pair of values would be compared, and answers that M26 does not concern would change.
Writing the fresh variable as N1: as correct, since the chain shows the names equal, but Nk is where the chain ends (`P = Q, Y = f(Q)`). A value that holds a query variable's own cell keeps that name either way (`Y = g(Q)`).

## Consequences

`X = Y, Y = Z.` answers `X = Y, Y = Z.`, `Y = Z, X = Y.` answers `Y = Z, Z = X.`, `findall(X-X, true, [P-Q]), Y = g(Q).` answers `X = X, P = Q, Y = g(Q).`, and `findall(f(X)-X, true, [Y-P]).` answers `X = X, Y = f(_G1), P = _G1.` Pasting an answer back gives the same sharing.
Only answers that left a name out change: `functor(T, f, 2), arg(1, T, A).` now also shows `A = _G1`, while `X = Y`, `X = f(Y)` and `Y = _G1, Z = [E|_G1]` stay. The translated code prints its answers with the same `format_answer`.
The REPL writes the goal it read with `_` kept as `_` unless `char_conversion` converts `_` (review L18), so anonymous variables are no names of the answer and add no `_G1 = _G2`: `f(_, X) = f(a, b).` answers `X = b.` mosatest's `solutions` is not changed. Tests: `check_answer_aliases` in tests/cpp/session_test.cpp.
