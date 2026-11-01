# M3: stable clause ids, generations and a first-argument index — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On a predicate of N clauses, calls, retract, retractall and clause/2 by a bound first argument cost the matching plus keyless clauses, an unbound call and asserta cost O(1) amortized, and one scan is linear (review M3). Baseline here (`build/mosa`, Release): scan 25k/50k clauses 169/656 ms; lookups 5k/20k 85/1,272 ms; retracts by key 5k/20k 812/12,922 ms; asserta 10k/40k 370/5,637 ms; `one :- p(_,_), !.` 5k/20k times 79/1,091 ms.

**Architecture:** (1) `backtrack` and the redo functions stop copying the top `Entry`. (2) Clauses move to a `std::deque` with ids that never change or repeat; scan candidates become ids, so asserta and removal renumber nothing. (3) Each clause records the generation it was added and removed; a scan keeps its generation, and an unbound-first-argument scan becomes a cursor instead of a list. (4) A lazily built first-argument index of ids serves calls, then (5) retract, retractall and clause/2; removal erases at the ends at once and in the middle by a batched sweep.

**Tech Stack:** C++20, CMake, `tests/cpp/harness.hpp`, `mosatest` (`tests/pl/`).

**Spec:** `docs/review-2026-10-31.md` M3; `docs/adr/0015-first-argument-index.md` (Proposed — the owner approves it before Task 1); ADR 0004, ADR 0008; ISO 7.5.4 (logical update view); `docs/SPEC.md` section 6.

## Global Constraints

- No code before ADR 0015 is approved; on approval set its Status (both languages), add it to `docs/adr/README.md` and `README.ja.md`, and make the status lines of ADR 0004 and 0008 name it.
- Functions ≤ 60 lines, files ≤ 800 lines (`database.cpp` 597 now; put the sweep and the id helpers in `src/machine/clause_store.cpp` if needed). No raw `new`/`delete`, C arrays, macros, `reinterpret_cast`; `-Wall -Wextra -Wpedantic` and `/W4` clean; no new dependency.
- Comments and commit messages in Japanese, one line: `[phase7] … （レビュー M3、ADR 0015、検証）`.
- Every call, clause/2 and retract must see the same clauses in the same order as today; the logical update view tests already in `tests/pl/database.pl` pass unchanged. Do not weaken tests; timing tests compare ratios only.
- mosac's generated clause selection (`src/codegen/emit.cpp` state 0, static predicates) is not changed; `enter_clause` and `native.cpp:274` keep taking positions.
- After each task: `ctest --test-dir build` and `MOSA_GC_STRESS=1 ctest --test-dir build -R "diff_tests_pl|prolog_tests|machine"` pass.

## Review Focus

1. Reconsult and failed consult (`load_clauses.cpp:128,139,154`, `consult.cpp:96`) of a predicate with an index or open scans: the next call by key sees exactly the new clauses — `m3_reconsult` (Task 4).
2. asserta and assertz during an open scan, crossing the 8-clause threshold: the scan sees the clauses of its call, the next call all of them, in order — `m3_asserta_open`, `m3_threshold_assertz` (Tasks 2 and 4).
3. Keys equal across representations: `0.0`/`-0.0`, `nan`, 2^62 (an `IntBox`), `1` vs `1.0`, `f/1` vs `f/2`, `[]` vs `[_|_]` — `m3_keys_*` (Task 4).
4. Removal while older scans are open (`p(X), retract(p(Y))`, retract inside a cursor scan, abolish during a scan, a removal at the front still reachable by an older scan): reachable clauses are still tried, then erased — `m3_sweep_*` (Tasks 3 and 5).
5. A cut or exception that drops scan entries before a sweep, and `--heap-max` stops inside a scan: no entry is left pointing at a missing id, `redo_clause` skips ids it cannot find — `m3_cut_then_retract` (Task 5).

---

### Task 1: Redo and backtrack read the top Entry in place

**Files:** Create `tests/cpp/index_scale_test.cpp`; modify `tests/tests.cmake` (after `choice_exit`), `src/machine/choice.cpp` (`backtrack`, `redo_clause`), `src/builtins/database.cpp` (`redo_retract`, `take_retract`, `push_retract`), `src/builtins/database_scan.cpp` (`redo_clause_scan`, `take_clause`, `push_scan`).

**Interfaces:**
- Produces: `check_linear(label, small_n, goal_of)` in the test: rebuilds `p/2` with `mk(N)` (assertz `p(I, I)`, I = 1..N), times the goal (min of 2 runs, `steady_clock`), fails when `t(4N) > 8·t(N) + 30 ms`. Later tasks add cases.
- `push_retract`/`push_scan` take `std::vector<std::size_t>&&`; `take_retract`/`take_clause` take the vector by value and move it on.

- [ ] **Step 1:** Write the test with `check_linear("scan", 25000, "(p(_, _), fail ; true).")`; register `index_scale`.
- [ ] **Step 2:** `cmake --build build --target index_scale_test && ./build/index_scale_test` → `FAIL: scan: …` (ratio ≈ 15).
- [ ] **Step 3:** `backtrack` takes `const Entry&` and copies `pick`, the three marks, `effect_id` and `armed` to locals before any `redo_*` (which may pop). `redo_clause` reads through a reference before popping or advancing. `redo_retract`/`redo_clause_scan` move `candidates` out before `pop_back`.
- [ ] **Step 4:** `./build/index_scale_test` passes; full `ctest` passes.
- [ ] **Step 5:** Commit `[phase7] 節のやり直しとバックトラックが選択点を候補ごと写さないようにする（レビュー M3、ADR 0015、index_scale の scan が旧コードで落ちる）`.

### Task 2: Clause store with stable ids

**Files:** Modify `src/machine/internal.hpp` (`Clause`, `Predicate`), create `src/machine/clause_store.cpp` (+ `CMakeLists.txt` source list); route every writer through it: `src/builtins/database.cpp:200,421,424`, `src/machine/load_clauses.cpp:74,128,139,154`, `src/machine/consult.cpp:85,96`, `src/machine/lgt_copy.cpp:129`; candidate producers and consumers: `src/machine/index.cpp`, `choice.cpp`, `database.cpp`, `database_scan.cpp`. Test: `index_scale_test.cpp`, `tests/pl/database.pl`.

**Interfaces:**
- `using ClauseList = std::deque<Clause>;` (`Predicate::clauses`, `Snapshot::clauses`, `ClauseBackup::clauses`). `Clause::id` (`std::size_t`). `Predicate::first_id`, `Predicate::next_id` start at `std::size_t{1} << 62`; ids in `clauses` are strictly increasing.
- `clause_store.cpp`: `void append_clause(Predicate&, Clause)`, `void prepend_clause(Predicate&, Clause)`, `void replace_clauses(Predicate&, ClauseList)` (keeps the ids it is given and widens `first_id`/`next_id` to cover them), `void clear_clauses(Predicate&)`, `void erase_clause_id(Predicate&, std::size_t id)`, `std::optional<std::size_t> clause_position(const Predicate&, std::size_t id)` (tries `id - clauses.front().id`, else `std::lower_bound`). `live_count` becomes O(1) from `Predicate::live`, kept by these helpers and by the dead-marking in `kill_clause`.
- `Entry::candidates` of `Pick::Clauses`, `Retract`, `ClauseScan` hold clause ids; `select_clauses` returns ids. `note_asserta` and the clause part of `note_removed` are deleted (the predicate-vector shift for `PredGen` stays).

- [ ] **Step 1:** Add `check_linear("asserta", 10000, …)` timing `mka(N)` (asserta `p(I, I)`, I = 1..N; the empty build is `mk(0)`), and `tests/pl/database.pl` guards: `m3_asserta_open` (`p(X)` over 3 clauses opens a scan, asserta a 4th inside it: `solutions` are the 3; the next call gives 4, new one first), `m3_retract_open_both` (retract a clause before and after the running one during a scan: same answers as today).
- [ ] **Step 2:** Run → `FAIL: asserta …` (ratio ≈ 15); `./build/mosatest tests/pl/database.pl` passes (guards of unchanged behavior).
- [ ] **Step 3:** Implement the store and the id candidates; `redo_clause`, `take_retract` and `take_clause` look clauses up with `clause_position` and skip an id that is gone. Check no writer is left: `grep -n "clauses\.\(push_back\|push_front\|insert\|erase\|clear\)\|clauses = " -r src` lists only `clause_store.cpp`.
- [ ] **Step 4:** `index_scale`, `ctest`, GC-stress subset pass.
- [ ] **Step 5:** Commit `[phase7] 節に変わらない番号を振り、選択点の候補を番号にして asserta と削除で付け直さないようにする（レビュー M3、ADR 0015、index_scale の asserta が旧コードで落ちる）`.

### Task 3: Generations and cursor scans

**Files:** Modify `src/machine/internal.hpp` (`Clause`, `Entry`, `Impl`), `clause_store.cpp`, `choice.cpp` (`call_pred` path at `:195-218`, `redo_clause`), `src/builtins/database.cpp` (`kill_clause`, `held_by_scan`, `purge_dead`, `abolish_open`, `erase_predicate`), `src/machine/engine.cpp` (`has_more` unchanged, check only), and every `.dead` reader (`index.cpp`, `database.cpp:323,359`, `database_scan.cpp:67`, `native.cpp:159`). Test: `index_scale_test.cpp`, `tests/pl/database.pl`.

**Interfaces:**
- `Impl::db_generation` (`std::uint64_t`), raised by every assert and removal. `Clause::born`, `Clause::died` (`kAlive = UINT64_MAX`); `bool Clause::dead() const { return died != kAlive; }` replaces the field; `bool visible(const Clause&, std::uint64_t g)` is `born <= g && g < died`.
- `Entry::generation` for the three scan picks; `Entry::cursor` (next id, `kNoClause` when done) with `Entry::by_cursor` for `Pick::Clauses`. A call with an unbound first argument, no argument, or `wraps_first` finds the first visible clause, looks ahead to the next, and pushes an entry only if one exists; `redo_clause` does the same from `cursor`. Retract scans still skip any dead clause (as today).
- `bool scan_reaches(const Entry&, std::size_t id, std::uint64_t died)`: the entry scans this predicate, `entry.generation < died`, and it has not passed the clause (cursor entry: `cursor <= id`; list entry: `id` is in `candidates[next..]`, found by binary search since ids ascend). `bool reachable(const Impl&, PredKey, const Clause&)` asks every open entry; it replaces `held_by_scan`. `kill_clause` sets `died` and erases the clause when it is not reachable (the retract's own entry has passed it).

- [ ] **Step 1:** Add `check_linear("unbound first call", 5000, …)` timing N calls of `one :- p(_, _), !.`, and guards `m3_sweep_cursor_retract` (inside `p(X, _)` over 9 clauses, retract the 5th and assertz a 10th: the scan gives the 9, the next call 9 without the 5th) and `m3_sweep_abolish_open`.
- [ ] **Step 2:** Run → `FAIL: unbound first call …` (ratio ≈ 14); database.pl passes.
- [ ] **Step 3:** Implement as in Interfaces.
- [ ] **Step 4:** `index_scale`, `ctest`, GC-stress subset pass.
- [ ] **Step 5:** Commit `[phase7] 節に足した世代と消した世代を持たせ、第1引数が未束縛の呼び出しを候補の列でなくカーソルで走査する（レビュー M3、ADR 0015、index_scale の unbound first call が旧コードで落ちる）`.

### Task 4: First-argument index for calls

**Files:** Modify `src/machine/internal.hpp` (`Predicate::index`), `src/machine/index.cpp`, `clause_store.cpp`. Test: `index_scale_test.cpp`, `tests/pl/database.pl`.

**Interfaces:**
- `struct IndexKey { KeyKind kind; std::uint64_t value; std::uint32_t arity; };` (Atom: atom id; Int: value; Float: bits with `-0.0`→`0.0` and every NaN→one NaN; List: 0; Fun: atom id, arity) and `struct FirstArgIndex { bool built = false; std::unordered_map<IndexKey, std::deque<std::size_t>, IndexKeyHash> buckets; std::deque<std::size_t> keyless; };` (ids, ascending). `Predicate::index` is `mutable`.
- Built by `select_clauses` on a bound first argument when `clauses.size() >= 8`; `append_clause`/`prepend_clause` push the id to its bucket's back/front; `replace_clauses`/`clear_clauses` and the sweep (Task 5) drop it; erasing a first or last clause pops the matching bucket end.
- `std::vector<std::size_t> key_candidates(const Impl&, const Predicate&, Cell first, std::uint64_t g)`: bucket merged with `keyless` by id, `visible` at `g`. Used by Task 5.

- [ ] **Step 1:** Add `check_linear("lookup by key", 5000, …)` (`p(I, _)` for I = 1..N) and guards, each on 9+ clauses: `m3_order_keyless` (keyed and variable first arguments interleaved), `m3_keys_zero`, `m3_keys_nan`, `m3_keys_big`, `m3_keys_int_float`, `m3_keys_arity`, `m3_keys_nil_list`, `m3_threshold_assertz` (7 clauses, scan open, assertz 2: the scan sees 7, the next call 9), `m3_reconsult` (consult a file written under `tests/tmp/` twice with different clauses, call by key).
- [ ] **Step 2:** Run → `FAIL: lookup by key …`; database.pl passes.
- [ ] **Step 3:** Implement as in Interfaces.
- [ ] **Step 4:** `index_scale`, `ctest`, GC-stress subset pass.
- [ ] **Step 5:** Commit `[phase7] 8 節以上の述語に第1引数の索引を持たせ、鍵での呼び出しを鍵の組と鍵の無い節だけにする（レビュー M3、ADR 0015、index_scale の lookup が旧コードで落ちる）`.

### Task 5: retract, retractall and clause/2 through the index; batched sweep

**Files:** Modify `src/builtins/database.cpp` (`collect_matches`, `kill_clause`, `purge_dead`, `abolish_open`), `src/builtins/database_scan.cpp` (`matching_clauses`, the `live_count` check at `:129`), `clause_store.cpp`, `internal.hpp` (`Predicate::dead_since`), `load_clauses.cpp:116`. Test: `index_scale_test.cpp`, `tests/pl/database.pl`.

**Interfaces:**
- Consumes `key_candidates` (Task 4), `scan_reaches`/`reachable` (Task 3).
- With a bound first argument the collectors unify only `key_candidates`. With an unbound one, retract and clause/2 no longer collect: they unify from a cursor up to the first match, then look ahead (unify and undo) to the next match and push a cursor `Retract`/`ClauseScan` entry only if one exists, so `has_more` and the REPL's `true.` stay as today; each clause is unified at most twice per scan.
- `void sweep_dead(Impl&, Predicate&)`: erases in one pass every dead clause that no open entry reaches (collect, per open entry of the predicate, its cursor or its untried ids once; do not call `reachable` per clause), drops the index, sets `dead_since = 0`. `kill_clause` erases at once when the clause is first or last and not reachable, otherwise `++dead_since`; `purge_dead` sweeps when `2 * dead_since > clauses.size()` or the predicate is abolished (abolish sweeps at once and erases the predicate if nothing is left, as now).

- [ ] **Step 1:** Add `check_linear` cases (2500 each): `retract by key`, `retractall by key`, `clause/2 by key`, `retract scan` (`(retract(p(_, _)), fail ; true)`), `retract from front` (N times `retract(p(_, _))`); guards `m3_sweep_held` (`p(X, _), retract(p(Y, _))` over 9+ clauses: same `solutions` as today), `m3_retract_counts` (`number_of_clauses` after retracting half), `m3_retract_reassert`, `m3_cut_then_retract` (`(p(X, _), ! ; true)` then retract all, then call).
- [ ] **Step 2:** Run → the five new cases FAIL; database.pl passes.
- [ ] **Step 3:** Implement: the two collectors use `key_candidates` when the head's first argument is bound, else every visible clause; then removal as in Interfaces.
- [ ] **Step 4:** `index_scale`, `ctest`, GC-stress subset pass.
- [ ] **Step 5:** Commit `[phase7] retract・retractall・clause/2 を第1引数の索引で絞り、途中の節はまとめて掃除する（レビュー M3、ADR 0015、index_scale の 5 件が旧コードで落ちる）`.

### Task 6: Measure, review, record

**Files:** `docs/bench.md`, `docs/review-2026-10-31.md` (+ `.ja.md`), `docs/adr/0004-clause-database.md`, `docs/adr/0008-logical-update-view.md` (+ `.ja.md`), `docs/STATUS.md`.

- [ ] **Step 1:** Build HEAD-before-Task-1 side by side and run the six `bench/` programs: within 5% (`docs/bench.md`, new section "Cost of the clause store and index" with the M3 times before and after).
- [ ] **Step 2:** Adversarial review of `main..HEAD` (adversarial-reviewer, model opus); fix CRITICAL/HIGH with a failing test first.
- [ ] **Step 3:** M3 `done (<commits>)` in both review files with what is left; STATUS (Next: M4); commit `[phase7] レビュー M3 を done にし、ベンチと STATUS を記す（文書のみ）`.
