English | [日本語](0014-catch-goal-as-data.ja.md)

Status: Approved (2026-11-01; review item M14, owner chose A in iso-notes 7.8.9, and A again for fixing mosac's `\+` and `M:G` in the same item)

## Context

7.8.9.1 runs the Goal and the Recovery of `catch/3` as `call/1` when the catch runs, so a variable in goal position that is bound to `!` by then is a cut local to that goal; `Build::goal` (`src/machine/compile.cpp`) translates both when the clause or the `call/1` goal is made, and the variable becomes a `Var` leaf that runs as `call(X)`, opaque to cut: `t(X) :- catch(((X, fail) ; true), _, true)` makes `t(!)` succeed, so do the Recovery form `catch(throw(e), _, ((X, fail) ; true))` and `call((Y = !, catch(((Y, fail) ; true), _, true)))`, while `G = catch(((X, fail) ; true), _, true), X = !, call(G)` fails (mosa and mosac).
mosac translates the data goal of `\+` and `M:G` ahead in the same way (`owned_goal` in `src/codegen/emit_body.cpp`), so `n(X) :- \+ ((X, fail) ; true)` makes `n(!)` fail in a mosac executable and succeed in mosa, where `\+/1` translates its goal when it runs.
ADR 0003 fixes the IR, and this gives meaning to `IrTag::Call`, which nothing builds today, so the ADR comes first.

## Decision

When `Build` translates `catch/3`, a Goal or Recovery that is a `,` `;` `->` or `*->` with an unbound variable in goal position below it (looking through those four only, as 7.6.2 does; a nested `catch/3` decides for itself) becomes an `IrTag::Call` node (atom `call`, arity 1, the subterm as data; in a goal built at run time the node points at the original term instead of a copy, so nested catches stay linear); a lone variable and a construct without such a variable stay compiled as today.
The node runs as a `call/1` leaf (`embody_leaf`, and in mosac the same `call_goal` as a written `call/1`), so the goal is translated with the bindings of the moment the catch runs and its cut barrier sits inside the catch, cutting the choice points the Goal (or Recovery) made, as 7.8.9.1 and 7.8.3 say; clause/2 and retract already show a `Call` node as its argument (`expose_start`), so the Goal is shown as written (7.6.2 c).
mosac's `owned_goal` refuses data with a variable in goal position, so `\+` and `M:G` with one go to the built-in as in mosa; the translator checks the term and mosac the IR, each in one walk with an explicit stack (ADR 0011), and mosac's undefined-call check still looks inside a `Call` node.

## Rejected alternatives

Keep every Goal and Recovery of `catch/3` as data: no analysis is needed, but every catch translates its goals again on each call, and mosac loses native code for catch goals, such as `lib/lists.pl:63`.
A new tag, or a flag on `Catch` meaning "the arguments are data": `open_catch`, `emit_catch` and every walker of `Catch` would change, while the engine, the codegen and `expose_start` already run and show `Call` as a call leaf. B (record the difference as a deviation) was turned down by the owner.

## Consequences

`t(!)`, the Recovery form and the `call((Y = !, ...))` form fail and `n(!)` succeeds, in mosa and in a mosac executable, as `call(G)` does; tests in `tests/pl/control.pl` (each also through `mosac`) cover the Goal, the Recovery, `\+`, `M:G`, a variable bound after the catch starts (`catch((X = !, ((X, fail) ; true)), _, true)` still succeeds, as call/1 converts `X` while it is unbound) and `clause/2` with `retract/1` on such a clause.
Such a catch translates its goal once per call, as `catch(G, ...)` with a variable `G` already does; other catches, `lib/` and the bench programs do not change; in such a goal a non-callable part now raises `type_error(callable, Goal)` before any of it runs, as `call/1` does (7.8.3.3).
Left out (review M15): without a variable, a non-callable part of a catch argument still runs the goals before it (`catch((write(a), 1), E, true)` writes `a`), and `call(catch((true, 1), E, true))` raises `type_error(callable, catch(...))` outside the catch, because `body_callable` looks into catch arguments.
