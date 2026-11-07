# Status

Phase 7 · `mosa 0.0.1 (phase 6)` · conformance 2136/2171 (98.4%) · main 2e2cbab, CI run 42155608727 passed all 8 jobs (2026-11-06)

- **In progress:** none.
- **Last done:** M29 (e59ded6; `~e` and `~f` write an infinity or a NaN as writeq does, whatever the digits, and a finite value with more digits than the heap limit in words stops at the limit instead of ending the process). Before it, M31 (ee33eaf), M28 (6ac3c60), L23 (63811c2), L22 (c473c46), L21 (f59a346), L20 (91ffdb4), L19 A1 (2aa1cdf), L19 (d8f4e1d), L17 (2fc7c7b, 7f2599b), L15, L14, L8, L12, L11, L2, L9, L10, L5, L6, L3, L7, L1, L4.
- **Next:** M30, M32 (sub_atom/5 takes cubic time; [review](review-2026-10-31.md)); then the manuals (`docs/manual/ja`, `en`) and release packages.
- **History:** the Status line of each finished item in the review, the ADRs, [bench.md](bench.md) and `git log`.

## Open
- **Not run yet:** Windows `chcp 932` and `chcp 437` (type `X = 親.` and `read(Y).` then `太郎.`; both must echo, and `halt.` must restore the code page; phase 5 Task 14 Step 3), NMake `--runtime-source`, aarch64 Linux.
- **Known limits:** the reader recurses once per nesting level (iso-notes 7.1); a Debug (/MTd) libmosa does not link with `mosac build` on MSVC; undefined-call warnings show the clause line, not the goal line, and can name another file after a nested consult replaces that predicate's clauses (M23 A2); library units are not traced under `--debug`; only Release is installed.
- **Tests:** `ctest -j` can fail with `suite scratch empty` (suite_path_test and conformance_io_test share `tests/tmp/suite`; serial passes), and the time ratios of index_scale and write_scale can fail under `-j` (pred_scale runs serially); the slowest deep and scale tests run at `MOSA_GC_STRESS=1000` or not under stress; after a goal that leaves a choice point, the runner runs later plain goals in a scratch machine.
- **Doubts:** consult/1 of the same file again under a choice point or inside findall fails silently; `std::clock` is wall time on MSVC; under `ulimit -v` below 4 GiB, catalog messages print their key; `statistics(heap, …)` leaves out the words of M17 state.
- **Doubts (limits and I/O):** the GC mark stack grows with left-nested chains such as `a+b+c+…`; a failed flush of `user_output` in close/1 is not a system_error, and lgt_io `close_alias` could close a standard stream (not reached today).
