# Open items

The details behind [STATUS.md](STATUS.md): what is unsure, decided, limited or not yet run.
English only, like STATUS.md. Keep lines near 100 characters, and delete an item once resolved.

## Questions to the owner

The repository has no text of 13211-1, so these wait for the owner. The default A is in force.

- 17 open questions of [review-2026-11-20.md](review-2026-11-20.md): H-39, M-06, M-08, M-16 and M-17
  (both fixed on A), M-18, M-23, M-24, M-25, M-26, M-56, M-60, L-07, L-20, L-25, L-27, L-33; with
  L-06 and M-59 below, 19. Each entry gives its A or B; default A keeps the current behaviour (H-09,
  H-12, H-18: owner's B).
- Review L-06 (fac56275): A, keep binding the newer of two unbound variables to the older (ADR 0019
  amendment 1; four REPL answer shapes name the older variable, `solutions` writes `X = Y` as `Y = X`),
  or B, bind left to right again (one revert of fac56275 and its test changes)? Default A.
- Branch review 2026-11-23 ([review-2026-11-23.md](review-2026-11-23.md)), default A keeps the
  current behaviour: R-M01 (count `passed` of `settle_shared` against the heap limit; B needs an ADR
  0029 amendment; since RV-m12 it drops repeats, so it holds one entry per distinct shared cell),
  R-M09 (B: read a lone `;` after a prefix operator once `;` is no operator, or bracket it when
  writing), R-M14 (B: also read a pretty-printed badge), R-L05 (B: refuse `system:import/1` and
  `system:use_module`), R-N03 (B: allow the infix `user:op/3` of a name a module hides at
  priority 0).
- Re-review RV-I01 (88bdeb90). A: under an open query, a then that only judges runs in scratch and
  keeps the open query, so `member(X,[1,2]), (X > 0 -> assertion(true)), X == 2` passes; but an
  assertion goal there that calls a library predicate fails with `existence_error`
  (`(X > 0 -> assertion(append([a],[b],[a,b])))`), which `701dff6e` passed. B: run every then on
  the main machine as `701dff6e` did, so that shape passes again and the `assertion(true)` shape
  fails again. Default A.
- Branch review R-L17. A: keep not counting the old lgtunit forms (`succeeds/1`, `fails/1`,
  `throws/2`; 36 suite files, 358 tests, phase 9), so a file holding only them still hides a load
  failure (5 bundled files). B: count them, which moves the ISO denominator, and also count a load
  failure with no failing test as one file row. The file-row change alone gives 2295/2338. Default A.
- Review H-40, the regression gate. Default A: it compares rates only, as SPEC section 8 says
  (B: it also refuses a smaller pass count or denominator).
- `max_integer`/`min_integer` under `bounded=false` (iso-notes 7.11.1.2).
  Default A: keep the int64 bounds.
- A Big beyond double range against a float (iso-notes 8.7). Default A: convert, `float_overflow`.
- `length/2`, ISO or not (iso-notes "Section 5 `length/2`", phase 11 Task 1). Default A:
  `length_2` counts in the library column, and the SPEC section 5 wording is not touched.
- Bindings of a cleanup that a cut runs (iso-notes "Section 7 Stage G `setup_call_cleanup/3`",
  phase 11 Task 1). Default A: ADR 0016 is kept, and `lgt_setup_call_cleanup_3_26` stays failing.
- Ask the owner before an I2 fix round 5 (see "Unsure points from phase 13").
- `spy/1` and `debugging/0`. Default A: no spy point on a goal with no ports (`debug_has_ports`),
  the debugger turned on only when one is set, names quoted, and `M:` where `user` would reach
  another predicate. `transcript_debug_spy_forms` and the `dbg_spy_*_off` rows pin it.
- `max_depth` (iso-notes "Debugger"). Default A: a cut part is never put in parentheses, lists
  with `ignore_ops(true)` keep the d+i-1 rule, and `quoted(true)` quotes the atom `'...'` while a
  depth is set. The `wd_minus_*`, `wd_*operator_atom_cut`, `wd_ignore_ops_*`, `wd_dots_*` rows.
- How the cleanups that `a` runs behave (ADR 0028), apart from their bindings. Default A: no
  ports; failures and errors dropped (`check_abort_cleanup_errors`); the debugger's on/off and
  mode kept as at the `a` (`check_abort_cleanup_keeps_mode`); a `halt` in them ends the process
  (checked by hand: `halt(3)` exits 3).
- How a message is made without loading on the user's machine (ADR 0007 amendment 3). Default A:
  on a fresh `render_only` machine, with the arguments, the operators and `japanese_punctuation`
  copied; the warnings keep their text (checked against the text before, in ja and en, and pinned by
  `transcript_debug_spy_after_warning` and `_ja`).
- Review M-59, the base of the links in release notes. Default A: `release_notes.sh` keeps the
  upstream URL, so a fork's notes link to the upstream tag. B: take it from
  `${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}`.

## Owner decisions in force

- Review N-07 (B, 2026-11-23): generated code keeps picking clauses by first argument with a linear
  `if` chain (see Known limits); A, an ADR for constant-time dispatch, was not taken.
- Review L-12 (A, 2026-11-23): an atom holding invalid UTF-8 (from argv) is written with each bad
  byte as the character of that code, as `atom_codes/2` sees it (`FF` as U+00FF); the output is
  well-formed UTF-8, but its bytes differ from the atom's. B (raw bytes, `\xFF\` in writeq) declined.
- Review L-16 (A, 2026-11-23): `meta_predicate` keeps reading a list that is not proper as the
  spec of `'.'/2` (the `m28_dot_*` rows of `tests/pl/modules.pl`), and `[]` as that of `'[]'/0`.
- The test presets' job count (A, 2026-11-23): a fixed 4 in all five presets, the same on every
  CMake (`"jobs": 0` is unbounded on 3.29 and later, 1 on 3.28; see Unsure). B (the processor
  count through an empty `CTEST_PARALLEL_LEVEL`, CMake 3.29 and later only) was declined.
- Review M-44 (A, 2026-11-22): each build works in a private directory and a finished one is put in
  place without a lock; B (a lock file with a stale-lock rule) was declined. Carried out with a
  `<hash>/current` pointer file instead of a renamed directory (review entry, 3616a8b1).
- Review H-30 (A, 2026-11-22): the cap of 2^20 variable cells in a clause's resume frames stays
  (ADR 0011 amendment 5, added while implementing); a clause past it runs as IR. B (a higher cap)
  and C (no cap) were declined.
- Review H-40 (A, 2026-11-20): `test/1` counts as `test(Name, true)`, so the ISO denominator moves
  from 2326 (SPEC phase 13) to 2333; the three multifile_1 failures have their iso-notes line.
  A file with an unreadable term (A, 2026-11-20): every readable case fails with the reason, and
  the unreadable term counts once as the file's line, so a broken file never drops out.
- Review M-50 (B, 2026-11-20): an object's `setup/0` and `cleanup/0` run once, before and after
  all its tests, as `run/0` in lgtunit.lgt does (c30993db). A `setup/0` that fails or throws fails
  every test of the file with actual `setup`, and `cleanup/0` then does not run.
- Review C-01 (B, 2026-11-20, 6080d441): at most 64 loads are open at once; a deeper one fails with
  `load_too_deep` (iso-notes 7.1), which also ends a file whose `initialization/1` goal
  consults the file itself. Second B (2026-11-20, 0aaf32bf): a file that a directive `include/1`,
  `ensure_loaded/1`, `use_module/1,2` or `reexport/1,2` reads counts too, so module chains stop at 64.
- Review H-41 (A, 2026-11-20): tests/pl has a fifth expectation, `ball(Ball)`, which subsumes a
  whole thrown term (SPEC section 8); the eight raw-ball rows keep their goals byte for byte.
- Phase 14 review questions (2026-11-18; iso-notes "Debugger (not in 13211-1)"):
  - A failed load also rolls `leash/1` back (B): the leashed set is in `SavedFlags`, restored with
    the on/off bit and the mode. Spy points stay.
  - `a` in an `initialization/1` goal keeps the clauses (A), also in a file a directive loaded, in
    either form. `check_abort_init` and `transcript_debug_abort_nested_*` pin it.
  - `a` runs the cleanups that the run leaves (A, adversarial review of PR #4), innermost run
    first. `check_abort_cleanup`, `check_abort_nested_cleanup` and `check_abort_directive_cleanup`.
  - Those cleanups see the bindings as an exception leaves them (B): each entry is undone to its
    marks first, so Setup's bindings show and Goal's do not, and no `catch/3` below sees their
    errors. `check_abort_cleanup_bindings` and `check_abort_cleanup_uncaught`.
  - The first message no longer loads `lists`, `format` and the message table into the program
    (B, adversarial review of PR #4, fixed in phase 14). `transcript_debug_spy_after_warning`.
  - A cyclic term beyond `max_depth(N)` is written as `...` (A): the depth wins for lists too;
    within the depth a cycle is still `_`. The `wd_cyclic_*` rows of `tests/pl/write.pl` pin it.
  - The commits after `5383fee4` (both adversarial reviews) stay local until the owner asks
    for the push (B, 2026-11-19).

- Phase 13, questions 1–3 (all A, 2026-11-16; iso-notes "Conditional compilation (not in
  13211-1)"; ADR 0027 approved):
  - `mosac` evaluates the conditions at build time, and the executable again at start-up.
  - The 39 `directives/conditional_compilation` items count in the library column.
  - A syntax error in a skipped part is passed over.
- Cyclic goals (2026-11-17, iso-notes 7.3.3):
  - Review M13 (A): a goal built at run time whose control structure leads back to itself
    raises `resource_error(cyclic_term)` before any part runs.
  - A cyclic `M:` chain (A): `strip_module/3`, `import/1`, the clause operations and
    `predicate_property/2` raise `resource_error(cyclic_term)`.
  - A cycle through a built-in meta-predicate's own goal argument stays an ordinary call (B).
  - `if/3` (A): a cycle from an `if/3` argument back to that `if/3` raises the error.
  - A cycle through two or more `if/3` nodes stays a known limit, as for `call((C *-> T ; E))` (B).
- Section 5 (A, 2026-11-15; iso-notes "Section 5 Importing a module's own predicate back under an
  alias"): a direct `m:use_module(own_file, [p/1 as q])` keeps importing nothing, while the same
  alias through a re-export is stored.
- Phase 12, questions 1–3 (all A, 2026-11-15; ADR 0026 approved):
  - A goal-time load failure prints the diagnostic and fails.
  - `mosac` bundles libraries named literally in goals.
  - The refusal to consult the same file again under a choice point or inside findall
    (`take_over`) stays (iso-notes "7.4.2 Consulting a path again under a choice point").
- 2026-11-13 (both A):
  - `halt/1` with an integer outside C++ `int` gives `representation_error(max_integer/min_integer)`
    (iso-notes 8.17.4).
  - `tab/1,2` with a positive Big writes spaces without end, as a too-large int64 does
    (iso-notes Stage G).
- `stack_deep_builtins` gets a 1200 s limit (A, 2026-11-17; see "Tests").

## Known limits of the cyclic-goal checks (iso-notes 7.3.3)

- A cycle through two or more `if/3` nodes runs as for `call((C *-> T ; E))` (owner's B).
- A cycle through a control construct inside a leaf in an argument still runs without end.
- `call/N`'s extra arguments (`X = call(call, X)`) are not followed (older than the decision).
- A cycle of leaves alone (`call/N`, `\+/1`, `once/1`, `M:G`) is found only up to a period of 32,
  as `wraps_back` walks 63 steps with Brent (CHANGELOG 0.4.1, Known limits).
- Review H-09 (owner's B): a late catch/3 Goal nested in another is checked at its root only, so a
  back edge from inside it is found when the outer catch runs, and that catch receives the error.
  The walk visits each construct once, so a back edge through a leaf to a construct it first met on
  another path is found the same way; a late Goal with a non-callable part is not walked (M15, M16).
- Cost of review H-08: past 63,488 slots a translation walks the goals of a part it would defer,
  also inside `call/N`, `\+/1`, `once/1` and catch/3, so k nested `call/1` layers that each pass
  the limit walk about k^2/2 layers: 16 layers of 64,000 goals take 1.4 s (0.6 s before), 32 4.6 s
  (1.2 s). A flat conjunction of 1,000,000 goals got faster (1.6 s to 1.3 s).
- From the Opus review of 748c831:
  - A1, kept as the decision's reading (row `m13_if_same_term`): another cell with the same three
    arguments is the same term by `==` and also raises, as in
    `X = if(fail, X, true), call(if(fail, X, true))`.
  - A2, left as the stricter side at the walk's limit: with exactly 63 `call/1` layers between
    then and the `if/3`, `if/3` raises where the `call` spelling needs one more step and runs on.

## Unsure points from phase 13

- From the review of I2 fix round 4 (low):
  - A2: `closed_doubtful` stays set for the rest of the load, also from a block in a library
    or in a file that a condition read.
  - A4: `own_module` is set only at the end of a text, so a text that a condition goal loads
    and that fails half way passes its mark up.
- Left by the owner's A after I2 (2026-11-16):
  - A directive in a branch not taken of an earlier block still counts as a skipped goal for
    a later block, which is then left to start-up (start-up fails as `mosa` does).
  - In a file that itself skipped a goal, a syntax error or refused clause is passed over in
    any branch, so an `else` after `:- if(fail). :- foo.` builds with `atom_length(a, 1).`
    (start-up fails as `mosa` does).
  - A directive after such a block that fails because of what the branch left (two imports
    that clash) still stops the build, while `mosa` runs. This is round 4's boundary.
  - An `if` nested in an `elif` branch still counts the goals skipped in the outer chain's
    earlier branches: `:- if(fail). :- foo. :- elif(true). :- if(true).` with the failing
    directive builds, while it stops without the inner `if`. Both match `mosa` at start-up.
- After Task 4: a skipped term that does not parse sets no `untaken_branch` (no test).

## Checked by hand only, or not tested

- The REPL answer form `P = _G1` (a class of one name whose variable is not that name's own, ADR 0019)
  has no test since L-06: unification never binds a query variable to a newer one, so no REPL query
  reached here builds it. The code stays for other bindings.
- `mosatest F` does not catch `ResourceLimit` in `run_exam` or `run_one` (`suite.cpp`), so a heap
  stop in one test would end the run. Its machine has the default 4 GiB heap, so this was not
  reached (review M-53, from code).
- A test file whose load stops at `halt/0,1` (86ac58e): `include`, nested consult and
  `:- if(halt(3)).` are untested; `mosac emit --units` and `run_file` are checked by hand
  only; the `.lgt` half of `--diff` was read from code.
- codegen's per-clause 16-bit resume limit and the close failures of `mosac emit` have no test.
- H-05 (f4c48d4a), read from code only: `hold_native_scans` on the live stack and through
  `abolish_stale` and `retire_older` (files written at run time are never native, so no test file
  reaches them). `run_goal` (`write_portray.cpp`) restores the stack from a copy, so a reload inside a
  portray hook drops the marks, and the old clause can be swept before the redo.
- Debugger test gaps from the 2026-11-18 review (next test task, one at a time): a leap transcript
  for each `spy/1` form other than `Name/Arity` (bare name, list, qualified; `check_spy_and_leash_state`
  pins only the set a bare name places); a multibyte unknown port command (`first_char` in
  `debug_port.cpp`); a port that stops only by the leash exception bit; degenerate command-at-port
  pairs (`s` at Redo, `f` at Fail, `r` at Call, `w` at Exception, an uncaught exception under
  trace); `check_skip_dropped` through a scripted `s` instead of injected state. The `.out` port
  sequences also have no oracle outside the implementation and the manual's checked examples.
- A cleanup that throws while `r` or `f` trims the stack above a box (Muse review of the fixes,
  2026-11-19): tested at the Redo port only (`transcript_debug_redo_r_catch` and `_f_catch`, H-07:
  one run, one Exception port, the outer catch/3 catches it), not at the Exit port.
- `backtrack` runs no `apply_catches` when it reaches `Pick::CallCleanup` (`choice.cpp`). The H-07
  triage found no way to reach it without first retrying an inner choice point, which re-arms.
- The message machine's heap limit is capped at the default (`render_message`, adversarial review
  2026-11-19): `--heap-max 70000000` and `130000000` were checked by hand on macOS only, since a
  test would depend on the address space.

## Runner and judge (`mosatest`)

- The `solutions([...])` judge compares each named variable's value as text, so it sees which
  way two variables are aliased: `X = Z` passes when `X` points to `Z`, not the other way.
  `bagof_rename` depends on it; review C-03 kept its answer by the choice of witness.
- A plain goal after a runner goal that binds variables inside an older saved value still
  does not carry back to that value (no suite case needs it).
- A merged plain run that leaves a choice point can pass a `deterministic` test that lgtunit
  reports as `non_deterministic_success`. `finish_det` (`lgt_judge.cpp`) asks the run for one
  more solution, which may fail, while lgtunit checks for a choice point at exit. For example,
  `test(x, deterministic) :- member(X, [1,2]), X == 1, ^^variant(X, 1).` passes
  (no suite case has this shape).
- The scratch machine gets the user's clauses (`plant_user_clauses`) but no library module, so a
  goal run there that calls `append/3`, `member/2` and the like fails with `existence_error`.
- After a goal that leaves a choice point, the runner runs later plain goals in a scratch machine.
  Their exceptions and halts come back (the ball as text, read again); a heap stop there aborts
  the test, as in the main machine (review H-36). Their bindings do not come back: in
  `test(t, true(Y == 1)) :- member(X, [1, 2]), assertion(true), Y = X.` the outcome does not see
  `Y`, so the test fails (found during M-49; not in the review; no suite case has this shape).
  Only after the open query's last solution do later goals run on the main machine again.
- A `forall/2` after a goal that leaves a choice point replaces that open query, so the choice
  point is dropped: `test(t, false) :- member(X, [1, 2]), (forall(true, assertion(true)) ; true),
  X == 2.` passes, while Prolog gives `true` (found during M-49, older than it; no suite case).
- A disjunction that ends the test body keeps the bindings of a left branch that fails after a
  runner goal: in `test(t, true(var(X))) :- (X = 1, ^^clean_text_output, fail ; true).` `X` stays
  `1`, so the test fails. `exec_disj` (`lgt_exec.cpp`) does not restore `saved`, while
  `exec_disj_then` does for `(A ; B), C` (found during M-54, older than it; no suite case).
- Under an open query, an if-then-else whose then may bind (a plain goal, `^^`, `forall/2`) runs the
  then on the main machine and drops the open query, as before the re-review: `test(t, false) :-
  member(X, [1,2]), (true -> assertion(true), Y = 1), X == 2.` passes. A then that only judges keeps
  it (RV-I01). Keeping it for a binding then needs scratch runs that bring their bindings back.
- `exists`, `all` and `deterministic` ask the last main query again even after an if-then-else
  committed: `test(t, exists(X == 2)) :- (member(X, [1,2]) -> variant(a, a)).` passes (fix check of
  the re-review, older than the range; no suite case).
- `(C *-> T ; E)` with a runner goal in `C` or `T` is still a disjunction, so a failing `T` falls
  to `E` (re-review RV-m01; with a runner goal only in `E` it is one marked query, as `->` is).
- A disjunction that is not a direct branch of a conjunction's left or of another disjunction is
  not entered again when later goals fail: `(fail -> true ; (X = 1 ; assertion(true), X = 2)),
  X == 2` fails (re-review RV-m02, older than the range). It needs continuations through every
  node.
- `restore_saved` reads each saved value alone, so two values that shared a variable lose it:
  `X = f(A), Y = g(A), assertion(true), Z = 1, variant(X-Y, f(B)-g(B))` fails (re-review RV-m04,
  older). Values saved at different times may reuse a `_G` name, so they cannot be read as one
  term.
- A gate or case condition decides `predicate_property/2` from the term as read, before the query
  runs, so a callable bound by an earlier goal is not seen: `(P = atom_length(_, _),
  predicate_property(P, built_in))` is false (`lgt_cond.cpp`; seen in a probe run; older than
  M-52; no suite case).
- The judge's `<`, `=<`, `>` and `>=` still compare an integer written as a `+(N)` or `-(N)`
  compound as a double (no suite item has one).
- Halts in the Logtalk paths give no reason (Opus re-review 2026-11-17, from code; no suite case):
  - `--suite` writes `load failed ` with an empty diagnostic for a `.lgt` whose load halts
    (`lgt_file.cpp:356`).
  - A gate or case condition that halts counts as false on both sides (`lgt_cond.cpp`).
  - `lgt_load_tester` goes on after a neighbour consult halts (`lgt_io.cpp:672`).

## Files to split before their next edit

- `src/machine/lgt_exec.cpp`, 749 lines (`wc -l`, 2026-11-24, after 88bdeb90): split before its next
  edit, for example the Pick node and the branch helpers into their own file. No other code or test
  file is at 730 lines or more (generated files and docs aside).

## Unsure

- The CI stalls and timeouts of 2026-11-22. The test presets had `"jobs": 0`, which CMake 3.29
  and later run unbounded (ctest(1) `--parallel`; 3.28 runs it as 1): the CI jobs started 139 to
  253 tests before the first one ended. readme-windows then timed out `mosac_run_exit_code`, its
  `_direct` and `manual_en_ja` together (runs 44986701989, 45023566371), and the macOS runner lost
  contact during Test with no log kept (runs 44983025478 on 685ff53c, before M-44; 45015884771;
  45028714188). The presets now run 4 at a time. That the macOS stalls share this cause (3 vCPU,
  7 GB) is inferred only, and the one local `mosac_build_parallel_direct` failure (output lost)
  fits it but is not proven. If either comes back at 4 jobs, keep the full `ctest` log.
- Translated `queens` is 8.1% slower than v0.1.4 with a byte-identical generated unit
  (5.4% more instructions, inside libmosa's evaluator; cause not found, bench.md).
  The final review's unmeasured candidate: `eval_int_bin` moved from an anonymous namespace of
  `arith.cpp` to `arith_int.cpp`, and with no LTO its callers can no longer inline it.
- No interpreted form reaches `run_halt` in `control.cpp`: all go through `arm_halt`,
  and both share `halt_code_problem`.
- A Big repeated in one clause is copied per occurrence in the IR, and gets one constant array
  per occurrence in generated code (bounded by `ir_budget`).
- `to_text` of 7^7^7 takes 12 s, and `parse_magnitude` of 3,000,000 hex digits 103 s
  (both quadratic, a design non-goal).
- Not in scope: output with no final newline runs into the answer (`?- write(a).` gives `atrue.`).
- Debugger: a `DebugExit` reached after its sentinel was cut (plan deviation 7) writes no exit port,
  since only the sentinel points at the original goal frame. Only translated code (`rt::cut_to`)
  can cut below a sentinel from inside its box, and executables have no `DebugConsole`.
- Debugger: an `a` typed in a cleanup that a cut or a closing query is running stops the cleanups
  chained after it from the same cut (they are frames in the continuation, not entries any more);
  in `r :- p, q, !.`, aborting in `q`'s cleanup skips `p`'s.
- Debugger: `Lib:G` from a non-library context gets a box only when `G` is one goal (`run_qualify`
  opens it, since later steps no longer know the caller). `lists:(G1, G2)` and a `lists:` closure
  that a library predicate calls (`maplist(lists:append([a]), Ls, Rs)`) stay hidden. So do
  `lists:lists:append(...)` (the inner `:` already runs in `lists`) and the goal that a built-in
  called as `lists:findall(X, G, L)` or `lists:once(G)` runs, since `G` then runs in `lists`.
  Changing that would change how `M1:M2:G` resolves or the box rule (ADR level); not in phase 14.
- Debugger: a goal whose context module is `system` gets no box, since that context means a goal a
  builtin runs for itself (findall/3's answer unification and the like). So `system:Goal` written
  by a user is not traced either.
- Debugger on Windows: not run there yet, the quoted `-DARGS` of the transcript tests. The manual's
  recording line uses a pipe (`type nul | mosa ...`); `mosa < NUL` is no longer a terminal (M-42),
  but the debugger's record through it has not been run on Windows.
- Debugger: notices (`% Spy point on ...`, abort) and port lines do not start on a fresh line after
  partial output.
- Debugger: a stopping port reads what `get_char/1` and the like left of a line as its command, as
  the `;` prompt does: `na` typed for `trace, get_char(C)` aborts at the Exit port with `a`. Only
  the characters `read/1` handed back are dropped (`transcript_debug_read_line`, 2026-11-19).
- Debugger: the session-side abort branches (`-g` exit 1, init main, startup file, `:- if`
  roll-back, reconsult restore) are checked by hand, not by a C++ test (the mode roll-back has
  `check_directive_nodebug_rollback`).
- Debugger: in a translation load, `Abort` alone is not treated like `Halt` (`load_cond.cpp`,
  `forget_failure` keeps `abort_now`). Unreachable today, since no translation has a console; noted
  by the 2026-11-18 review as a latent asymmetry, not fixed.

## Not run yet

- The 64-load bound (review C-01) was measured on macOS Release only. `consult_cycle_test` goes
  64 loads deep (goal, directive and mixed chains) on the Windows runner (1 MiB default stack)
  and under ASan; seen only after a push.
- A clean Windows machine. The `windows-2022` runner has more tools than the README lists;
  that nothing else is needed rests on CMake having no `find_package` or `find_program`.
- A Japanese-locale Windows (ANSI code page 932, Japanese UI). `readme-windows` changes only the
  console code page.
- Windows `chcp 932` and `chcp 437` in the REPL: type `X = 親.`, and `read(Y).` then `太郎.`.
  Both must echo, and `halt.` must restore the code page (CHANGELOG says not verified).
- NMake `--runtime-source`, also under a non-ASCII path.
- aarch64 Linux.
- The TextEdit and About This Mac labels of manual chapters 2 and 4 on a real Mac.

## Known limits

General:
- `walk_suite` (`iso_suite.cpp`) stops without a note when a directory step fails, so the suite's
  denominator would shrink silently (review L-40). Such a failure was not caused on any OS here.
- A directive goal that recovers from a kept consult failure and then fails for another reason keeps
  the outer load (branch review R-L06): the load cannot tell that failure from `:- consult(F).`,
  whose failure is the consult's own. Open; it needs a way to see that a goal ran after the failed
  consult.
- At `halt`, of two streams on one file the text of the one opened later stays, even when the other
  wrote last (R-M08, re-review RV-m10); `close/1` and `flush_output/1` let the latest act win. ISO
  leaves several streams on one file implementation dependent; a write counter would be needed.
- Reconsult (review L11 A7 and A9, review N-11): the import of a predicate that a module file stopped
  exporting stays in the importer after the file is consulted again; and a reload that makes a static
  predicate dynamic (`p(1).`, then `:- dynamic(p/1). p(2).`) fails with `static_to_dynamic` and keeps
  `p(1)`. Both are older than L11 and left; the record was only in docs/review-2026-10-31.md.

- Two cyclic terms with coprime periods still unify or compare in time quadratic in the periods,
  until the pairs pass the heap limit (ADR 0010 amendment 4, review H-02). Union-find over compound
  cells would make `=` and `==` linear; not shown to keep `compare/3`'s order. Left for later.
- Under `--heap-max 16`, `length(L, 300000)` reaches 48 MB RSS, and with a recursive fill and
  `copy_term/2` 151 MB: something outside the heap that the limit does not count (seen in H-02).
- The standard order of cyclic terms (ADR 0029, review H-04): a comparison that decides after
  passing a shared subterm also compares the levels above that difference, and an equal or
  undecided depth-first walk of a deep term keeps 32 bytes a pair for its path and frames (counted
  past the heap words in use). Measured: `L == M` of two 1,000,000-integer lists peaks at 361 MB RSS
  under `--heap-max 64` (360 MB before, from building the lists); the p=2000 `X == Y` of H-02 stops
  at 17 MB RSS under `--heap-max 16` (32 MB before). `mosatest --bench` was not run for H-04; three
  micro benchmarks (`==` of copies, sorts of compound terms, sorts sharing a subterm) were.
- setof/3 orders groups whose witnesses differ only in variables by the caller's cells (7.2.1
  leaves the order of variables implementation dependent). `wg17_setof_3_47` wants solution
  order or its reverse; it passes because the last witness stands for an aliased class (C-03).
- `mosac build` of a source clause nested 100,000 deep works, but clang takes about 80 minutes
  and 1.2 GB on it (iso-notes 7.1).
- A clause runs as IR, not generated code, when the goals mosac writes into one function (a
  condition, the left of `;`, the goal of `\+`, `catch/3` or `M:G`) nest more than 64 deep, when
  it has over 65,535 resume points, or when its frames carry over 2^20 variable cells (H-30).
  A frame carries every variable the rest of the body uses, so a body whose deeper levels each
  add a variable (nested `catch(G, _, R)`) makes code that grows with the square of its depth.
- A `catch/3` recovery that throws again nests natively in the interpreter: under a 1 MiB stack,
  `catch(throw(x), _, catch(throw(x), _, ...))` 2,000 deep ends in SIGSEGV in mosa (1,000 passes,
  2026-11-22, found during H-30). A mosac executable crashed on the same clause, which it runs as IR.
- Grammar rules given to assertz/1 are translated only through `expand_term/2` of
  `library(dcg)`; `term_expansion/2` and `goal_expansion/2` do not exist.
- A Debug (/MTd) libmosa does not link with `mosac build` on MSVC.
- A translated predicate picks its clauses by first argument with a linear chain of `if` (review
  N-07, owner's B), so a call takes time in the position of its key, and each `kKey` array repeats
  every keyless clause. 2,000 facts `f(k<i>, i)`, 300,000 calls: the last key 682 ms, the first 97 ms
  (mosa 343 and 345 ms). 100 keyless clauses before 4,000 keyed ones make the generated unit
  12.2 MB, not 3.8 MB. Constant-time dispatch would change the generated code of ADR 0009, so it
  needs an ADR.
- Undefined-call warnings show the clause line, not the goal line, and can name another file
  after a nested consult replaces that predicate's clauses (M23 A2).
- Library units are not traced under `--debug`.
- At translation, a goal `use_module(library(N))` is read on the program's machine after the
  program's own load, even in a clause never called or an `:- if` branch not taken (run time reads
  it only when the goal runs). A user module named like N or like a library N loads (`lists`
  through `format`) is replaced there and runs as IR; output is unchanged. Only the message
  catalogs moved to a separate machine (H-28). The other order, a program file that takes over a
  library's module after loading it, keeps the library bundled (H-28's adversarial review).
- A newline in mosac's own directory name no longer breaks the generated CMakeLists.txt (H-29),
  but CMake cuts cache values at a newline. The first configure of a build tree finds the runtime;
  a later configure of the same tree reads the cut `MOSA_RUNTIME_DIR` and `MOSA_RUNTIME_LIBRARY`.
- A name over 2,048 bytes in mosac's tables of atoms, callees and entries is a `char` array, not a
  string literal (H-31). A translated predicate's name is still a C++ identifier as long as
  `p<mangle(name)>_<arity>`, three characters a byte for a byte that is no letter or digit (ADR
  0030 keeps the form). MSVC tells identifiers apart by their first 2,048 characters only, about
  227 Japanese characters of a name, so such a predicate and its `_run` and `_c1` functions may
  clash there. Not checked (CI only, found during H-31); changing the form needs an ADR.
- The build cache (M-44): a killed build's `.mosac-cache/<hash>/w-*` and a killed publish's
  `current.tmp-*` stay until a build of the same key (hit or miss) a day later, or `--clean`; a
  build that runs longer than a day can be removed by another build of its key. A killed copy
  leaves `<out>.tmp-*` next to `-o`. On a file system without hard links (exFAT), two builds that
  finish together both publish, and the first one's `w-*` stays unnamed until a day later; that path
  has no test. `publish_dir` checks that `current` exists before it reads it; reversing that order
  opens a window of microseconds that `cache_place` does not catch (seen by mutation). Windows
  sharing modes (a reader of `current`, a loser's `remove_all`) are seen by CI only.
- Only Release is installed.
- The REPL writes prompts and answers past `user_output`, so its line position stays where the
  query left it. After a query whose output has no final newline, the next query's leading `~N`
  writes an empty line (`?- format("~Nb~n").` after `?- write(a).`, seen under a pty).
  Both are in CHANGELOG 0.2.2, Known limits.

Big integers and the heap limit:

- `^` estimates `exponent × bit_length(base)`, so near the heap limit it can refuse a result
  that fits (up to 2× for a power-of-two base).
- A Big step's heap check counts garbage, so it can stop at the heap limit before a safe-point
  GC would have freed room.
- Evaluation copies Big operands, so reading a Big stops once it takes about half the limit,
  and a Big result at about a third.
- An integer literal is parsed in full off the heap before `Heap::big` checks the limit
  (`lexer_literal.cpp`, Muse review): 3,000,000 hex digits under `--heap-max 1` peaked at
  23 MB RSS before the heap-limit stop. A 3,000,000-character atom peaks at 24 MB and a comment
  that long at 12 MB, so most of it is the reader's own memory, which is outside the limit for
  any long token.
- `'$format_radix'/4` builds the digit text off the heap before making the atom, so a Big near
  the heap limit in radix 2 can use about 64× its word count before the limit triggers
  (as `~d` does through `number_chars`).

Phase 12 (CHANGELOG 0.3.0, Known limits):

- A failed goal load's diagnostic bypasses the `user_error` stream, so its column does not move.
- In a `mosac build` executable, `module_property(M, file(F))` gives the file's embed key
  resolved against the working directory (`library/Name.pl` for a bundled library, the path
  relative to the common ancestor of the main file and the program's own files for those files,
  as SPEC section 5 says), which can differ from the interpreter's path.
- `mosac` still warns that an unqualified `maplist/3` after a goal `use_module(library(apply))`
  is undefined, though the executable runs (goal libraries are read as
  `use_module(library(N), [])`).
- A `M:G` into a module that no directive loads (one loaded only from a goal, or `lists` not
  loaded at all) goes through `:/2` at run time in a `mosac` executable.
- `mosac` does not bundle a file that a goal load names by a plain path, so the executable reads
  it from the working directory.

Phase 13: see CHANGELOG 0.4.0, Known limits.

## Tests

- `ctest --preset` runs in parallel (review item 5), 4 at a time (owner's A, 2026-11-23).
- `tests/cpp/windows_process_test.ps1` is ASCII only, comments included, against the rule of
  Japanese comments: Windows PowerShell 5.1 reads a file without a BOM in the ANSI code page, and
  under 932 a UTF-8 byte at the end of a comment can take the newline. Its Japanese text is built
  from code points.
- On macOS runners `stack_deep_builtins` takes 250–510 s. Two push runs on ae22e43
  (43912835169, and the first attempt of 43912873254) went over the 600 s default `TIME_LIMIT`
  of `small_stack.cmake` at different items, while its PR run passed in 492 s and the rerun in
  466 s. Since 2026-11-17 (owner's A) that test alone gets 1200 s and a ctest `TIMEOUT` of 1500 s.
- Time ratios remain load-sensitive. index_scale's scan ratio failed once serially on a macOS
  runner (release run 42878532812, attempt 1).
- The slowest deep and scale tests run at `MOSA_GC_STRESS=1000`, or not under stress.
- `tests/cpp/transcript_test.cmake` gave each mosa 20 s. Debugger transcripts went over it on CI
  with their full output: Windows (run 44292746612), linux-gc-stress (44268611104), and
  linux-asan (36 and 23 tests in 44428606659 and 44428614255). Since `7868abab` the limit is 180 s.
  In the green runs 44435291291 and 44435300641 the slowest was 44 s (linux-asan, `spy_forms`),
  and Windows takes about 80 s per test. A hang at end of input is not ruled out until more
  Windows runs pass, since one Windows failure had its full output.
- `mosa_stdin`, `mosa_utf8_pipe` and `mosa_japanese_arg` also had a 20 s child limit; the first
  two timed out mid-output on Windows (run 44442297643). Since `9083f681` they have 180 s too;
  runs 44449589046 and 44449596124 were green in all jobs.
- On linux-asan, message_ops went from 21 s (`5383fee4`) to 79 s while the runner was about 1.5×
  slower overall. A likely cause is the fresh render machine per message (`f8000797`, ADR 0007
  amendment 3). `debugger` takes 122–135 s of its 600 s on linux-asan and linux-gc-stress.

## Doubts

Load rollback:

- The load rollback's catch handlers copy vectors and strings (`reject_load`, `rollback_preds`,
  `hand_up_snapshots`), so a `bad_alloc` there leaves a partial rollback. This happens only when
  the process itself is out of memory (Opus re-review A3).
- The second Muse review adds three: `reject_load` truncates the heap after `rollback_preds`;
  a throw in `hand_over()` inside `finish_and_hand_over`'s catch replaces the first exception;
  and `run_aside`'s `formal_text` runs outside its try. Guards that swallow a second throw were
  not added, since they would hide a partial rollback.
- These have no test that is red without them: in `run_aside`, the launch inside the try (A4),
  and the catch's `restore_state` and drops of `pending_cleanups` and `native_next` (as
  `drop_run` does). The load's rollback truncates the heap and the open query's backtrack undoes
  the trail; a cut's pending cleanup lives only within one step; `native_next` is set only by
  translated code; and a heap limit is not timed to hit `launch`.
- That no catch that takes the heap limit goes on reading a load (the premise of A6) was checked
  over all 46 `catch` sites on 2026-11-15.

Left from the Muse review of the second-review fixes (`run-mwmjtumy-zmw1rd`, 2026-11-19; numbers are
its Low findings):

- Deferred, real: 25, `writeq_minus_postfix` in `tests/pl/write.pl` leaks `op(200, xf, '~~')` if
  the read-back throws (phase 7 test); 34, ADR 0007 amendment 2 has no Context/Decision labels
  (phase 10 text); 35, a comment in `load_directive.cpp` that says what, not why (the file was split
  by bb97f7b8 and is off the split list).
- Not reproduced: 9, a skip left after a heap stop in a directive; the failed load restores the
  debugger's on/off and mode (owner B), with the debugger off or on before the directive.
- By design or already covered: 5, a shared (DAG) term written in full is exponential in any
  `writeq`; 7, the fallback writes each argument by itself, as ADR 0007 says; 10, a non-heap throw
  from the port-name renderer was not shown to happen; 15, the scale bound (per-run port counts were
  added for Medium 2); 16, `check_abort_pending_cleanup`'s comment says why it is white box; 36,
  "Review Focus N" names the section of the phase plan, as in other tests.

Platforms and locales:

- `mosac dist` zips with Japanese names were read by `cmake -E tar` and `unzip -t` only,
  not by Explorer.
- libarchive tools in a C locale misname Japanese tar entries (GNU tar keeps the bytes).
- `std::clock` is wall time on MSVC.
- Under `ulimit -v` below 4 GiB, catalog messages print their key.

Left since 0.1.0:

- The exit-status sentence of manual chapter 14.
- The `|| true` guard in release.yml.
- `release_notes.sh` rewrites links inside code spans.
- Child processes on Windows inherit every inheritable handle.

Muse reviews:

- C++ review guesses not acted on: the 5 `const_cast`s in heap, unify and native are kept;
  archive.cpp's zip size fields truncate past 4 GiB (no zip64).
- Findings not taken: `^` inside a bagof goal body (7.1.1.3 counts only the leading `^`);
  the close/2 error order (7.12 leaves it implementation dependent); cyclic terms in the judge's
  `lgt_same_shape` (a hypothesis with no suite case).

Other:

- `statistics(heap, …)` leaves out the words of M17 state.
- The GC mark stack grows with left-nested chains such as `a+b+c+…`.
- A failed flush of `user_output` in close/1 is not a system_error.
- lgt_io's `close_alias` could close a standard stream (not reached today).
- `(|)` reads as an atom only while `|` is an operator (this reading of Cor.3 6.3.4.3).
- A reader frame is about 104 bytes with libc++, and more with libstdc++ and MSVC. Reading one
  1,000,000-deep `f(…)` peaks at 240 MB, outside `--heap-max`, and a consult Reader keeps the
  frame vector's capacity until the consult ends. Slimming the frame, with operator and functor
  as atom ids, is for a later phase.
- In directives only an over-limit arity gets a Formal (`representation_error(max_arity)`).
  Other bad indicators (`x//foo`, `x//(-1)`, `1//0`, `_//1`) print the generic
  「述語指示子ではありません」, and the 1024 limit is a bare literal in several files rather
  than one shared `kMaxArity`.
