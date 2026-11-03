English | [日本語](0016-call-cleanup.ja.md)

Status: Approved (2026-11-03; the owner chose A for review M11, which put this ADR before `setup_call_cleanup/3`, and asked for M11 to be implemented in the same request)

## Context

`setup_call_cleanup(S, G, C)` (SPEC section 7 stage G, review M11, iso-notes "Section 7 Stage G utilities") must run `C` exactly once when `G` exits with no choice points left, fails, raises, or has its choice points cut, as SWI-Prolog does (`boot/init.pl`, `pl-wam.c`).
Mosa has no hook at any of these points: a cut is `trim_stack` (also from generated code through `rt::cut_to`), failure is `backtrack`, an exception is `propagate_throw`, and a finished query or directive drops its stack without looking at it.
`trim_stack` runs in the middle of a step whose caller then sets `ip`, so it cannot start a Prolog goal itself, and running `C` in a nested `run_loop` there would let a GC move the cells that generated code holds in C++ locals.

## Decision

The builtin runs `once(S)` and then the internal `'$call_cleanup'(G, C)`, which checks `C` (instantiation or callable error, before arming), pushes an entry `Pick::CallCleanup` whose `pattern` is `C` and `module` the context module, and calls `G` with a `Ctrl::CleanupExit` frame after it; `G`'s errors come after arming.
`CleanupExit` with the entry on top, and every `trim_stack` that removes such entries (top first), add `C` to `Impl::pending_cleanups` (GC roots); at the top of each `run_loop` iteration a non-empty list becomes goal frames `(call(C) -> true ; true)` before the current `ip`, before `fail` when the machine has failed, or before `throw(Ball)` when an exception is waiting, and `propagate_throw` leaves the stack alone while the list is non-empty, so a throw in the same step as the cut cannot cut the heap under `C`; when one cut removes several, each runs under `catch/3` and the first exception is rethrown after all of them (SWI's `discardChoicesAfter` also runs them all); the interpreter's `->` commit makes the then-branch a frame while the list is non-empty instead of launching it in the same step, and generated code tests `rt::cleanup_pending` after each cut (and after the commit of `->`) and, when the rest would run in the same function (arithmetic, a construct, `\+`, `throw/1`), moves it to the next step, so `C` sees the bindings at the cut and runs before a `halt` after it; `backtrack` reaching the entry pops it, queues `C` and fails after it, and `propagate_throw` reaching it restores the entry's marks and runs `(catch(call(C), _, true) -> true ; true), throw(Ball)`, so `C` sees the bindings of `S` but not those of `G`, and the goal's exception wins.
A query, a directive or a portray goal that stops with choice points left first cuts them and runs the queued cleanups in the same loop, and so are a `-g` goal, a toplevel answer and `initialization(G, main)` after their answer (SWI's `'$run_init_goal'` and toplevel cut there too); an exception from them is the directive's, portray's or goal's exception and is ignored when a new query of the embedding API closes the old one; `halt` and a heap-limit stop run no cleanup, also not when the next query closes the stopped one.

## Rejected alternatives

Running `C` at once in a nested `run_loop` from `trim_stack`, `backtrack` and `propagate_throw`: generated code holds heap cells in C++ locals across `rt::cut_to`, and a GC in the nested loop would move them.
A per-entry counter so that `trim_stack` skips the scan: entries leave the stack by `resize`, `pop_back`, `clear` and stack swaps in a dozen places; the scan only reads the `pick` of entries that are being destroyed anyway.
Ending the generated function at every cut: every `N > 0, !, N1 is N - 1` would pay a frame and a trip through `run_loop` although no cleanup is armed.

## Consequences

A cut runs `C` before anything after it, in the interpreter and in generated code alike; generated code pays one test of a vector per cut, and one more resume state per cut (or `->` then-branch) followed by goals that run in the same function, states for the same goal sequence being shared so that the count stays linear in the cuts.
`run_loop` reads one more vector per step and `trim_stack` reads the `pick` of each entry it removes; `has_more` and the consult busy check treat `Pick::CallCleanup` like `Pick::Catch`, so it is not a solution and does not block redefinition.
Tests: `scc_*` in `tests/pl/utilities.pl` (every finish reason, bindings, exceptions in `G` and `C`, cut from `once/1`, `->`, `\+`, findall, a throw, an evaluation error and arithmetic right after a cut or `->` in a clause body, several cleanups from one cut), `tests/pl/cleanup_dir.pl`, `machine_test` (closing by the next query, not after `halt`, before a `halt` in the then-branch), `session_test` (`-g`, toplevel, main) and `tests/mosac/cleanup_main.pl`, run by mosa and by mosac executables through `diff_tests_pl`, and with `MOSA_GC_STRESS=1`.
