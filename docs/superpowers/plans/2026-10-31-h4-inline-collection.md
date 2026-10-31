# H4: collections without native recursion (ADR 0013)

Goal: nested findall/3, findall/4, bagof/3, setof/3 never recurse in C++; 100,000 levels under a 1 MiB stack in mosa and in a mosac executable.

## Tasks

1. RED: `tests/stack/deep_collect.pl` (+ `.expected`), ctest `stack_deep_collect` (small_stack.cmake) — findall/3, findall/4, bagof/3, setof/3 nested 100,000 deep, plus a throw from the bottom caught at the top. Fails today with rc 139.
2. RED: `tests/mosac/deep_collect_test.cmake`, ctest `mosac_deep_collect` — `mosac build` the same file, run the executable under `ulimit -s 1024`.
3. Guards in `tests/pl/solutions.pl` that must stay green (also in `mosatest --diff`): exception through a collection to an outer catch with the outer scan intact, cut inside the goal stays local, collection inside `\+` and inside `catch/3` recovery, bagof backtracking over groups after a nested collection, assert inside the goal (logical update view), op/3 undone after findall (current behaviour, M4).
4. GREEN: `internal.hpp` — `Pick::Collect`, `Ctrl::Collect`, `struct Collector` (virtual `visit`, `gc_pass`), `Entry::collect`; `gc.cpp` — `GcVisitor::collect` once per pass, called from `GcVisitor::entry`; `engine.cpp` step_control → `collect_step`; `choice.cpp` backtrack → `finish_collect`; `solutions.cpp` — `run_solutions` pushes the entry and launches, drop `Saved`/`collect`/park/rearm/unwind.
5. Verify: ctest (all), `MOSA_GC_STRESS=1` diff_tests_pl, `mosatest --bench` within 5% (docs/bench.md), regenerate agreement.
6. Docs: review H4 done, STATUS, iso-notes 7.1 (en/ja) names the remaining nested runs.
