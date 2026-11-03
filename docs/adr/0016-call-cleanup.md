English | [日本語](0016-call-cleanup.ja.md)

Status: Approved (2026-11-03; the owner chose A for review M11, which put this ADR before `setup_call_cleanup/3`, and asked for M11 to be implemented in the same request)

## Context

`setup_call_cleanup(S, G, C)` (SPEC section 7 stage G, review M11, iso-notes "Section 7 Stage G utilities") must run `C` exactly once when `G` exits with no choice points left, fails, raises, or has its choice points cut, as SWI-Prolog does (`boot/init.pl`, `pl-wam.c`).
Mosa has no hook at any of these points: a cut is `trim_stack` (also from generated code through `rt::cut_to`), failure is `backtrack`, an exception is `propagate_throw`, and a finished query or directive drops its stack without looking at it.
`trim_stack` runs in the middle of a step whose caller then sets `ip`, so it cannot start a Prolog goal itself, and running `C` in a nested `run_loop` there would let a GC move the cells that generated code holds in C++ locals.

## Decision

The builtin runs `once(S)` and then the internal `'$call_cleanup'(G, C)`, which checks `C` (instantiation or callable error, before arming), pushes an entry `Pick::CallCleanup` whose `pattern` is `C` and `module` the context module, and calls `G` with a `Ctrl::CleanupExit` frame after it; `G`'s errors come after arming.
`CleanupExit` with the entry on top, and every `trim_stack` that removes such entries (top first), add `C` to `Impl::pending_cleanups` (GC roots); at the top of each `run_loop` iteration a non-empty list becomes goal frames `(call(C) -> true ; true)` before the current `ip`, before `fail` when the machine has failed, or before `throw(Ball)` when an exception is waiting; `backtrack` reaching the entry pops it, queues `C` and fails after it, and `propagate_throw` reaching it restores the entry's marks and runs `(catch(call(C), _, true) -> true ; true), throw(Ball)`, so `C` sees the bindings of `S` but not those of `G`, and the first exception wins.
A query, a directive or a portray goal that stops with choice points left first cuts them and runs the queued cleanups in the same loop; an exception from them is the directive's or portray's exception and is ignored when a new query closes the old one; `halt` and a heap-limit stop run no cleanup.

## Rejected alternatives

Running `C` at once in a nested `run_loop` from `trim_stack`, `backtrack` and `propagate_throw`: generated code holds heap cells in C++ locals across `rt::cut_to`, and a GC in the nested loop would move them.
A per-entry counter so that `trim_stack` skips the scan: entries leave the stack by `resize`, `pop_back`, `clear` and stack swaps in a dozen places; the scan only reads the `pick` of entries that are being destroyed anyway.

## Consequences

A cut runs `C` before the next goal frame, so in generated code inline unifications and arithmetic between the cut and the next call happen before `C` (nothing a program can observe except an arithmetic error, after which `C` still runs before the error goes on); the interpreter has no such gap.
`run_loop` reads one more vector per step and `trim_stack` reads the `pick` of each entry it removes; `has_more` and the consult busy check treat `Pick::CallCleanup` like `Pick::Catch`, so it is not a solution and does not block redefinition.
Tests: `scc_*` in `tests/pl/utilities.pl` (every finish reason, bindings, exceptions in `G` and `C`, cut from `once/1`, `->`, `\+`, findall, a directive and the toplevel), run by mosa and by mosac executables through `diff_tests_pl`, and with `MOSA_GC_STRESS=1`.
