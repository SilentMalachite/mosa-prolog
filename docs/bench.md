English | [日本語](bench.ja.md)

# Benchmarks

`mosatest --bench <file> [--count <N>] [--work <directory>] [--cmake <path>]` runs `bench(N)` five times in the interpreter (`mosa <file> -- N`) and five times in the executable that `mosac emit` + CMake (Release) builds from the same file, then prints the wall-clock median of each side and the ratio (interpreter time divided by translated time). Each run is one whole process, from start to exit, so both sides include process start-up and the loading of the source in the same way. `N` is the same on both sides (default 20000). The emitted project, its build and the logs go under `--work` (default: `mosatest-bench` next to the `mosatest` executable, in the build tree); only the child directory `mosatest-bench-out` is removed and recreated. The programs in `bench/` define `bench(N)` as a failure-driven loop, check their first solution before running it, and read `N` from the flag `argv`; `statistics/2` is not used (phase 7).

## nrev

`bench/nrev.pl`: naive reverse of a 30-element list (`nrev/2` and `app/3`), repeated `N` times by failure-driven backtracking. One `nrev` of 30 elements is 496 logical inferences. The run checks first that the first solution equals the reversed list.

| Date | Machine / OS | Build | N | Interpreter (ms) | Translated (ms) | Ratio |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-29 | Apple M1 Max, macOS 27.2, Apple clang 21.0 | Release | 20000 | 3147 | 149 | 21.2 |

Command, from the repository root: `LANG=C build/mosatest --bench bench/nrev.pl --work tests/tmp/bench`.

The translated side used the code `mosac emit` produces as of phase 6 Task 6 without any change for this measurement: the first-argument dispatch (`select_clauses` rule), native-to-native calls by callee number, and head unification that reads the frame arguments directly (ADR 0009) were already in place from Task 4. The phase 6 exit criterion (SPEC section 9, at least 3 times) is met.
