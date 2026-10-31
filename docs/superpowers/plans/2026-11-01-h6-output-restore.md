# H6: current output survives a heap-limit stop — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** After a heap-limit stop (or any C++ exception) inside `with_output_to/2` or a `portray/1` hook, the next query writes to the stream that was current before the call, and no capture stream stays open.

**Architecture:** `ResourceLimit` is a C++ exception that unwinds from the heap through `run_goal` up to `Machine::query`'s caller, skipping the hand-written `machine.current_out = saved` lines in `run_with` and `apply_portray`. Replace both with one RAII guard `OutputScope` (restore on every exit, close an owned capture stream, fall back to `user_output` if the saved stream was closed meanwhile). `Machine::query` also resets a current input/output that names a closed or missing stream, as a last line of defence.

**Tech Stack:** C++20, CMake, the project harness (`tests/cpp/harness.hpp`).

**Spec:** `docs/review-2026-10-31.md` H6; `docs/SPEC.md` section 6 (no C++ exceptions for Prolog `throw/1`; RAII is the house style).

## Global Constraints

- Functions ≤ 60 lines, files ≤ 800 lines (`write_pred.cpp` is 746 now; the guard must stay small or move out).
- No raw `new`/`delete`, macros, `#ifdef` outside `src/platform/`; `-Wall -Wextra -Wpedantic` clean.
- Comments and commit messages in Japanese; commit line `[phase7] … （レビュー H6、検証方法）`.
- Tests must not be weakened; new test must finish under `MOSA_GC_STRESS=1` or be moved to the 1000 stage in `.github/workflows/ci.yml` (both lists).
- Temporary files only under `tests/tmp/`.

## Review Focus

1. Nested redirects (`with_output_to` inside `with_output_to`, `print` with portray inside `with_output_to`) stopped at the inner level: every level unwinds, the outermost saved stream comes back.
2. `with_output_to(S, G)` with a real stream sink: the guard must not close the user's stream `S` on unwind.
3. `set_output/1` inside the redirected goal, then a stop: still restored to the stream from before the call.
4. The stream that was current before the call is closed inside the goal (normal exit, no stop): current output must become `user_output`, not a closed stream.
5. Normal exits (success, failure, caught Prolog exception, `halt`) keep their current behavior — existing tests plus explicit checks.

---

### Task 1: Adversarial tests (RED)

**Files:**
- Create: `tests/cpp/output_stop_test.cpp`
- Modify: `tests/tests.cmake` (register `output_stop_test`, `MOSA_SOURCE_DIR` definition, test name `output_stop`)

**Interfaces:**
- Consumes: `mosa::run_session(args, input, tty, heap_limit_words)` (`include/mosa/repl.hpp`), `mosa::Machine::create(limit)`, `mosa::ResourceLimit`, `engine_impl` (`src/machine/internal.hpp`, as in `gc_test.cpp`).

Heap limit for every case: `std::size_t{16} << 17` words (16 MiB); the hog goal is `length(_, 100000000)`.

- [ ] **Step 1: Write the tests** — each a `check_*` function, each asserting with `mosa_test::check`:
  - `session_with_output_to`: `-g 'with_output_to(atom(A),(write(hello),length(L,100000000)))'`, tty input `write(visible), nl.\n` → `out` contains `visible`, `out` does not contain `hello`.
  - `session_portray`: file in `tests/tmp/` with `portray(big) :- length(_,100000000).`, `-g 'print(f(big))'`, input `write(visible), nl.\n` → `out` contains `visible`, `err` does not contain `existence_error`.
  - `api_*` cases on one `Machine` with the 16 MiB limit, each "stop goal" must throw `ResourceLimit`, then the follow-up query must succeed:
    - plain `with_output_to(atom(_), (write(x), length(_,100000000)))` → follow-up `current_output(S), stream_property(S, alias(user_output))`.
    - no stray open streams: `findall(S, stream_property(S, mode(_)), L), length(L, N)` gives the same N before and after three consecutive stops.
    - nested `with_output_to(atom(_), with_output_to(codes(_), length(_,100000000)))`.
    - portray inside capture: `with_output_to(string(_), print(f(big)))` with `portray(big)` loaded.
    - `set_output(user_error)` inside the goal before the hog.
    - stream sink: `open('<tmp>/output_stop.txt', write, S, [alias(os_sink)])`, then stop inside `with_output_to(os_sink, length(_,100000000))` (if the stream form is not accepted, use `S` bound in the same query); follow-up `write(os_sink, after), close(os_sink)` succeeds and the file contains `after`.
  - `closed_saved_falls_back` (no stop): `open(<tmp>, write, S), set_output(S), with_output_to(atom(_), close(S)), current_output(C), stream_property(C, alias(user_output))` succeeds.
  - `normal_exits_unchanged`: `with_output_to(atom(A), write(x)), A == x`; `\+ with_output_to(atom(_), fail)`; `catch(with_output_to(atom(_), throw(e)), e, true)` then current output is `user_output`.
  - `query_resets_closed_current`: via `engine_impl`, push nothing new; set `impl.streams[k].closed = true` for a memory stream index `k` made by `with_output_to(atom(_), true)` (the closed capture slot is `streams.size()-1` after that query), set `impl.current_out = k` and `impl.current_in = k`, then `current_output(S), stream_property(S, alias(user_output)), current_input(I), stream_property(I, alias(user_input))` succeeds.

- [ ] **Step 2: Build and run, confirm RED**

Run: `cmake --build build && ./build/output_stop_test`
Expected: failures in `session_with_output_to`, `session_portray`, the `api_*` stop cases, `closed_saved_falls_back`, `query_resets_closed_current`; `normal_exits_unchanged` passes.

### Task 2: `OutputScope` and the query reset (GREEN)

**Files:**
- Modify: `src/builtins/write_pred.cpp` (`run_with` 694-712, `apply_portray` 448-478)
- Modify: `src/machine/engine.cpp` (`Machine::query`)

**Interfaces:**
- Produces: in `write_pred.cpp`'s anonymous namespace
  `class OutputScope { OutputScope(Impl&, std::size_t index, bool owns); ~OutputScope(); void restore(); }` — constructor saves `current_out` and sets it to `index`; `restore()` is idempotent and sets `current_out` back to the saved index, or `1` when the saved stream is now closed; the destructor calls `restore()` and, if `owns`, marks `streams[index].closed = true`.

- [ ] **Step 1:** Implement `OutputScope`. In `run_with` construct it with `owns = sink.kind != SinkKind::Stream`, call `restore()` before `finish_with`. In `apply_portray` construct it with `owns = false` (the hook's destructor owns the scratch stream) and call `restore()` right after `run_goal`.
- [ ] **Step 2:** In `Machine::query`, before reading the goal: if `current_out` is out of range or closed set it to `1`; same for `current_in` → `0`.
- [ ] **Step 3:** Run `./build/output_stop_test` → all pass; `MOSA_GC_STRESS=1 ./build/output_stop_test` → pass in under a minute (else add `output_stop` to the 1000 stage in `ci.yml` and to the stage-1 exclusion).
- [ ] **Step 4:** Full `ctest --test-dir build` and `MOSA_GC_STRESS=1 ctest --test-dir build -R diff_tests_pl` pass.
- [ ] **Step 5:** Commit code + test.

### Task 3: Review, docs, STATUS

- [ ] **Step 1:** Adversarial review (opus) of the diff; fix CRITICAL/HIGH findings with a test first.
- [ ] **Step 2:** Mark H6 done in `docs/review-2026-10-31.md` and `.ja.md`; update `docs/STATUS.md`; commit.
