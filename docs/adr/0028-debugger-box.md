English | [日本語](0028-debugger-box.ja.md)

Status: Approved (2026-11-18, owner; the owner's request for the phase 14 plan counts as approval, and on the same day the owner asked for the plan to be implemented). SPEC section 9 gives phase 14 (debugger); this ADR fixes how the engine catches the ports of a call, who may make a box, and how `abort` stops a run.

## Context

The trampoline engine reads a goal at one place, `step`, but has no place that catches an exit, a redo or a failure. A continuation is not only in `Goal`: the entry and the retry of a clause, and the retry of a non-deterministic built-in, read it again from word 3 of the goal frame.
So swapping the `cont` of `Goal` does not lead to an exit frame; `catch/3` already catches the exit and the unwinding of a goal with a choice point and an exit frame (ADR 0012).
Port lines and commands need I/O that only the `mosa` session has, and `mosatest`, the executables of `mosac build` and an embedded `Machine` must not change their answers or output.

## Decision

When `step` has read a goal to show, and only when the session gave the machine a `DebugConsole`, it pushes a sentinel `Pick::DebugBox`, an exit frame `Ctrl::DebugExit` and a copy of the goal frame whose word 3 is that exit frame, and runs the copy in the same step (`run_boxed`), so `Impl` keeps no heap address over a safe point. Exit is caught at `DebugExit` (with a `Pick::DebugRedo` for redo when a choice point is left inside), fail when backtracking reaches the sentinel, exception when `propagate_throw` removes a sentinel that has not yet exited (an exited sentinel and `Pick::DebugRedo` are removed silently).
`r` and `f` cut above the sentinel with `trim_stack` and leave the rest to backtracking, and cleanups run as in ADR 0016; `nodebug/0` and `n` return to leap. Control constructs, `call/1..8`, the nine debugger predicates, `$` names and calls from library modules have no box; in leap only a spied predicate gets one.
`abort` is a new stop kind, `StopKind::Abort`, of the same form as `halt`, seen by `run_loop` right after `halt_now`; in a load it rolls the load back as a load error does.

## Rejected alternatives

A meta-interpreter for the traced goals: the same clause would run in two ways, which may differ in cut, cleanups and errors, against goal 5 of SPEC section 1 (the same results).
Rewriting clauses to call port goals: the database changes (`clause/2`) and the cost stays when the debugger is off.
Swapping only the `cont` of `Goal`: it does not reach `DebugExit` on the paths that read word 3 again.

## Consequences

With the debugger off, or with no `DebugConsole`, `step` costs one branch; in leap only spied calls get a box, so a run up to a spy point is near normal speed (owner question 2, decision A; the bench must stay within 1.2 times).
While tracing, a tail call keeps its box and a deep recursion uses memory in proportion to its depth; the depth and `g` count only open boxes, and inside a directive only those opened in it.
Tests: transcripts under `tests/cpp/data/` (`transcript_test.cmake`), `tests/pl/debugger.pl`, the `check_*` functions in the C++ tests, `MOSA_GC_STRESS`, the `mosac` standalone ctest; phase 14 plan Tasks 2 to 10.
