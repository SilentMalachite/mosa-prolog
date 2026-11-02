English | [日本語](0013-inline-collection.ja.md)

Status: Approved (2026-10-31; the owner asked for the plan of review item H4 to be implemented in the same request)

## Context

`findall/3`, `findall/4`, `bagof/3` and `setof/3` run their goal by calling `run_loop` again from C++ (`collect` in `src/builtins/solutions.cpp`), so each nesting level costs a native frame: `d(N,s(L)) :- N1 is N-1, findall(X, d(N1,X), [L]).` ends in SIGSEGV at about 6,000 levels with an 8 MiB stack and below 1,000 with 1 MiB, bagof at about 10,000 (review 2026-10-31 H4).
Each level also copies the whole choice stack (`save_exec`) and walks it twice to disarm and re-arm the catches below, so n nested collections cost O(n²).
ADR 0011 rejected a depth limit because ISO 7.1 sets none; the collection is the remaining construct that recurses per Prolog level.

## Decision

A collection runs inside the caller's `run_loop`, the way `\+` does: after the argument checks it pushes an `Entry` with `Pick::Collect` whose `collect` field (a shared `Collector`: template, witnesses, `^` variables, the variable numbering, the solutions so far, the call frame) is a GC root through `GcVisitor::collect`, visited once per pass like a shared env, and launches the goal with the cut barrier above that entry and a `Ctrl::Collect` frame as continuation.
Reaching `Ctrl::Collect` records the solution (`remember`) and fails; backtracking to the `Collect` entry restores the state, pops it and binds the answer through `bind_found` or `bind_answers`, which launch at the call's continuation as today. An exception passes the entry like any other choice point, so the outer catches stay armed and are no longer disarmed and re-armed, and `halt/0` stops the machine at once.
Generated code reaches the collection through `call_goal` and `run_builtin` as before, so it needs no change; the other nested `run_loop` callers (portray hooks, directives, `with_output_to/2`, the Logtalk copy check) are left as they are.

## Rejected alternatives

A depth limit with `resource_error`: ISO 7.1 sets none (ADR 0011), and the native stack left at a given depth differs per OS (1 MiB on Windows), so the limit would be either too low or unsafe.
Re-entering the builtin through its own frame to finish (the frame identifies the collector): it re-reads the goal and depends on frame identity; a separate `Pick` keeps the finish in `backtrack` next to `redo_not`.

## Consequences

Nested collections no longer use the native stack and run in time linear in the depth (about 1.1 KB per level: the `Entry` and `Collector` live outside the heap, so `--heap-max` does not count them, as for any other choice point); `tests/stack/deep_collect.pl` runs findall/3, findall/4, bagof/3, setof/3 and a throw from the bottom 100,000 levels deep under 1 MiB, in mosa and in a mosac executable.
Answers, error terms, the order of side effects and the undoing of op/3 at the end of a collection (review M4) do not change (M4 was later decided as A on 2026-11-02: op/3 is never undone, iso-notes 8.14.3); `Machine::choice_depth()` counts one `Collect` entry per running collection instead of one `Barrier`.
A collection interleaved with a portray hook, `with_output_to/2` or a directive still recurses once per pair (about 1,000 pairs under 1 MiB end in SIGSEGV), recorded in iso-notes 7.1; `Saved`, `park_callers`, `disarm_below`, `rearm_below` and `unwind_throw` leave `solutions.cpp`; `Pick::Barrier` stays for the other nested runs.
