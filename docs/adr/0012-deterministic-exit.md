English | [日本語](0012-deterministic-exit.ja.md)

Status: Approved (2026-10-31; the owner asked for the plan of review item H3 to be implemented in the same request)

## Context

When the goal of `catch/3` exits, `disarm_catch` only clears `armed`, and when the condition of `*->` succeeds, SoftCommit only sets `cond_ok`, so both `Entry` records stay on the choice stack even when nothing was left above them (review 2026-10-31 H3).
A deterministic loop such as `lw(N) :- catch(true,_,true), N1 is N-1, lw(N1).` therefore keeps one `Entry` per step; their `pattern`, `env` and `else_goal` are GC roots, so `lw(300000)` stops at `--heap-max 8` while the same loop without `catch/3` runs 1,000,000 steps.
`has_more` and the nested-run helpers walk the whole stack, so every leftover entry also costs time later.

## Decision

When the goal of `catch/3` exits (`Ctrl::CatchExit`, `rt::catch_exit`) and its `Catch` entry is the top of the stack, `disarm_catch` pops it with `trim_stack`; otherwise it disarms it as before and records it in `catch_log`.
When the condition of `*->` succeeds (`Ctrl::SoftCommit`, `rt::soft_commit`) and its else entry is the top, the entry is popped; otherwise `cond_ok` is set as before. `rt::soft_commit` searches from the top and stops at the first match.
This is safe because the choice points made by the goal are exactly the entries above it: with none, execution can never re-enter the goal, so the exit frame never runs again, and backtracking, `has_more` and `propagate_throw` already skip a disarmed catch or a committed else.

## Rejected alternatives

Popping also when only dead entries (a disarmed catch, a committed else) are above: it needs a scan per exit, and nested deterministic code already pops the inner entry first, so the outer one is then on top.
Removing dead entries in the GC: the stack indexes kept in `cut`, `Extra::trim_to`, `catch_log` and generated code would all need rewriting, for no gain over popping at exit.

## Consequences

Deterministic loops through `catch/3` and `*->` keep the choice stack, `catch_log` and the heap flat, in the interpreter and in generated code; when choice points remain above the entry, nothing changes. `soft_id` and `next_soft` become 64-bit, since such a loop can now run past 2^31 `*->`.
Backtracking, cuts and exceptions behave as before. Three things a program can see now match `once/1` and `->`, which never left an entry: an op/3 call after such an exit inside a directive is no longer recorded against the dead entry, so an outer backtrack keeps it (review M4 still decides whether op/3 is undone at all); consult no longer refuses to redefine a predicate with `predicate_running` after a deterministic `*->`; `Machine::choice_depth()` counts fewer entries.
Tests: `choice_exit` (2^16 words, 1,000,000 steps each), `mosac_heap_limit` (mosa and a mosac executable at 1 MiB), and `control.pl` (`op_exit_same`, `soft_exit_consult`, and guards for redo, cut and throw).
