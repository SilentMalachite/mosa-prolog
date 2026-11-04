# Status

Phase 7 · `mosa 0.0.1 (phase 6)` · conformance 2136/2171 (98.4%) · main 5c92c9f, CI run 41831451308 passed all 8 jobs (2026-11-04)

- **In progress:** none.
- **Last done:** review M22: `write_terms` writes the bindings of one toplevel answer, and mosatest's `solutions` comparison, with one numbering of made-up names (`L = [_G1,_G2], M = [_G3]`); tests `check_answer_numbering`, `check_runner_answer_numbering`, `m22_*`. Before it, M25 (ADR 0018).
- **Next:** review items M26 (decision), M27, L1-L12, L14, L15, L17, L18 ([review](review-2026-10-31.md)); then the manuals (`docs/manual/ja`, `en`) and release packages.
- **History:** the Status line of each finished item in the review, the ADRs, [bench.md](bench.md) and `git log`.

## Open
- **Not run yet:** Windows `chcp 932` and `chcp 437` (type `X = 親.` and `read(Y).` then `太郎.`; both must echo, and `halt.` must restore the code page; phase 5 Task 14 Step 3), NMake `--runtime-source`, aarch64 Linux.
- **Known limits:** the reader recurses once per nesting level (iso-notes 7.1); a Debug (/MTd) libmosa does not link with `mosac build` on MSVC; undefined-call diagnostics show the clause line, not the goal line, and can name another file after a nested consult replaces that predicate's clauses (M23 A2); library units are not traced under `--debug`; only Release is installed.
- **Tests:** `ctest -j` can fail with `suite scratch empty` (suite_path_test and conformance_io_test share `tests/tmp/suite`; serial passes), and the time ratios of index_scale and write_scale can fail under `-j` (pred_scale runs serially); the slowest deep and scale tests run at `MOSA_GC_STRESS=1000` or not under stress; after a goal that leaves a choice point, the runner runs later plain goals in a scratch machine.
- **Doubts:** consult/1 of the same file again under a choice point or inside findall fails silently; `std::clock` is wall time on MSVC; under `ulimit -v` below 4 GiB, catalog messages print their key; `statistics(heap, …)` leaves out the words of M17 state.
- **Doubts (limits and I/O):** the GC mark stack grows with left-nested chains such as `a+b+c+…`; over 65,535 variables in one goal or clause overflow the uint16 IR slot; a failed flush of `user_output` in close/1 is not a system_error, and lgt_io `close_alias` could close a standard stream (not reached today).
