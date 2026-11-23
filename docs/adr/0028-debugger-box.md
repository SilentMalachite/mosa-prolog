English | [日本語](0028-debugger-box.ja.md)

Status: Approved (2026-11-18, owner; the owner's request for the phase 14 plan counts as approval, and on the same day the owner asked for the plan to be implemented; revised the same day after the adversarial review of PR #4, where the owner chose A: `abort` runs the cleanups, and a `Lib:G` written outside a library is traced, then B: those cleanups see the bindings as an exception leaves them). SPEC section 9 gives phase 14 (debugger); this ADR fixes how the engine catches the ports of a call, who may make a box, and how `abort` stops a run.

## Context

The trampoline engine reads a goal at one place, `step`, but has no place that catches an exit, a redo or a failure. A continuation is not only in `Goal`: the entry and the retry of a clause, and the retry of a non-deterministic built-in, read it again from word 3 of the goal frame.
So swapping the `cont` of `Goal` does not lead to an exit frame; `catch/3` already catches the exit and the unwinding of a goal with a choice point and an exit frame (ADR 0012).
Port lines and commands need I/O that only the `mosa` session has, and `mosatest`, the executables of `mosac build` and an embedded `Machine` must not change their answers or output.

## Decision

When `step` has read a goal to show, and only when the session gave the machine a `DebugConsole`, it pushes a sentinel `Pick::DebugBox`, an exit frame `Ctrl::DebugExit` and a copy of the goal frame whose word 3 is that exit frame, and runs the copy in the same step (`run_boxed`), so `Impl` keeps no heap address over a safe point. Exit is caught at `DebugExit` (with a `Pick::DebugRedo` for redo when a choice point is left inside), fail when backtracking reaches the sentinel, exception when `propagate_throw` removes a sentinel that has not yet exited (an exited sentinel and `Pick::DebugRedo` are removed silently).
`r` and `f` cut above the sentinel with `trim_stack` and leave the rest to backtracking, and cleanups run as in ADR 0016; `nodebug/0` and `n` return to leap. Control constructs, `call/1..8`, the nine debugger predicates, `$` names, calls made inside library modules (a `Lib:G` written outside one gives its single goal `G` a box) and the helper goals a built-in launches (they run in the `system` context or as `'$once'`/`'$not'`) have no box; in leap only a spied predicate gets one. A `DebugExit` whose sentinel was cut writes no Exit port.
`abort` is a new stop kind, `StopKind::Abort`, seen by `run_loop` right after `halt_now`; unlike `halt`, each run it stops first runs the cleanups of the `setup_call_cleanup/3` entries above its floor (the `Pick::Barrier` of portray and `with_output_to/2`, else the bottom) as an exception passing them does (ADR 0016): the top entry first, each after undoing to that entry's marks, so it sees the bindings of Setup and not those of Goal, and cleanups a cut has already queued before them, with the bindings of the cut; then it cuts the rest down to the floor; innermost run first, with no ports, each under its own `catch/3` so that no `catch/3` below sees its error, ignoring their failures and errors and keeping the debugger's on/off bit and mode, while a `halt` in them ends the process (owner decisions of 2026-11-18: A, `abort` runs them; B, with the bindings of an exception; the rest is a default A in open-items); in a `:- G` or `:- if(G)` directive it rolls the load back as a load error does, restoring the on/off bit, the mode and the leashed ports of the debugger together (owner decision B, 2026-11-18); in an `initialization/1` goal it follows the rule of that goal (the load fails, the clauses stay).

## Rejected alternatives

A meta-interpreter for the traced goals: the same clause would run in two ways, which may differ in cut, cleanups and errors, against goal 5 of SPEC section 1 (the same results).
Rewriting clauses to call port goals: the database changes (`clause/2`) and the cost stays when the debugger is off.
Swapping only the `cont` of `Goal`: it does not reach `DebugExit` on the paths that read word 3 again; running the cleanups of `abort` with the bindings of the moment (as a closed query): what they see would depend on the port where `a` was typed.

## Consequences

With the debugger off, or with no `DebugConsole`, `step` costs one branch; in leap only spied calls get a box, so a run up to a spy point is near normal speed (owner question 2, decision A; the bench must stay within 1.2 times).
While tracing, a tail call keeps its box and a deep recursion uses memory in proportion to its depth; the depth and `g` count only open boxes, and inside a directive only those opened in it; a box keeps the depth it had when it opened, since the open boxes outside it cannot change while it is open, and a new box reads its depth from a table of the open boxes (kept like a stack, set aside with the frames in a directive), so no port scans the stack even under many choice points.
Tests: transcripts under `tests/cpp/data/` (`transcript_test.cmake`), `tests/pl/debugger.pl`, the `check_*` functions in the C++ tests, `MOSA_GC_STRESS`, the `mosac` standalone ctest; phase 14 plan Tasks 2 to 10.

## Later fixes

Fixes made after the decision, kept apart so that each section above stays within three lines (review R-L24).
Fixed later (review H-14): a spy point is kept under the defining module and the defining name, so a predicate imported with `as` is spied under its alias, under its own name and under `M:Name`, and `spy(Alias)` finds it; leap's cheap name check also lets a call through when its context imports the name as an alias, and a per-module flag (`Module::aliased`) keeps that look at the imports away from modules without aliases.
Fixed later (reviews R-M03 and R-M04): a spy point is shown by the name placed when that name still reaches the same predicate from `user`, and otherwise by the defining name, with the module when `user` reaches another predicate by it; a later spy/1 of the predicate that an earlier key now reaches replaces that key, so leap stops at calls by the defining name.
Fixed later (reviews RV-I04 and RV-m06): a key placed before its predicate was visible is marked deferred, and leap's name check lets every call of that arity through to the full match, so it stops at calls by the defining name without a second spy/1; the mark is dropped once the key's own module defines the name. `debugging/0` and `nospy/1` show two keys that now reach the same predicate once, by the key placed first.
