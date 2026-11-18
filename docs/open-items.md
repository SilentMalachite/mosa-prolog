# Open items

The details behind [STATUS.md](STATUS.md): what is unsure, decided, limited or not yet run.
English only, like STATUS.md. Keep lines near 100 characters, and delete an item once resolved.

## Questions to the owner

The repository has no text of 13211-1, so these wait for the owner. The default A is in force.

- `max_integer`/`min_integer` under `bounded=false` (iso-notes 7.11.1.2).
  Default A: keep the int64 bounds.
- A Big beyond double range against a float (iso-notes 8.7). Default A: convert, `float_overflow`.
- `length/2`, ISO or not (iso-notes "Section 5 `length/2`", phase 11 Task 1). Default A:
  `length_2` counts in the library column, and the SPEC section 5 wording is not touched.
- Bindings of a cleanup that a cut runs (iso-notes "Section 7 Stage G `setup_call_cleanup/3`",
  phase 11 Task 1). Default A: ADR 0016 is kept, and `lgt_setup_call_cleanup_3_26` stays failing.
- Ask the owner before an I2 fix round 5 (see "Unsure points from phase 13").
- Debugger `a` inside an `initialization/1` goal (phase 14 Task 7). A: the load fails and keeps its
  clauses, as an exception there does (iso-notes 7.4.2 initialization/1). B: roll the load back, as
  SPEC section 6 and ADR 0028 say. B cannot restore predicates a reconsult has already removed:
  ADR 0020 removes them before the goals run, and `abolish_stale` erases them. Default A is built and
  pinned by `check_abort_init` in `tests/cpp/debugger_test.cpp`. A `:- G` directive rolls back.

## Owner decisions in force

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

- A test file whose load stops at `halt/0,1` (86ac58e): `include`, nested consult and
  `:- if(halt(3)).` are untested; `mosac emit --units` and `run_file` are checked by hand
  only; the `.lgt` half of `--diff` was read from code.
- codegen's per-clause 16-bit resume limit and the close failures of `mosac emit` have no test.

## Runner and judge (`mosatest`)

- A plain goal after a runner goal that binds variables inside an older saved value still
  does not carry back to that value (no suite case needs it).
- A merged plain run that leaves a choice point can pass a `deterministic` test that lgtunit
  reports as `non_deterministic_success`. `finish_det` (`lgt_judge.cpp`) asks the run for one
  more solution, which may fail, while lgtunit checks for a choice point at exit. For example,
  `test(x, deterministic) :- member(X, [1,2]), X == 1, ^^variant(X, 1).` passes
  (no suite case has this shape).
- After a goal that leaves a choice point, the runner runs later plain goals in a scratch machine.
- The judge's `<`, `=<`, `>` and `>=` still compare an integer written as a `+(N)` or `-(N)`
  compound as a double (no suite item has one).
- Halts in the Logtalk paths give no reason (Opus re-review 2026-11-17, from code; no suite case):
  - `--suite` writes `load failed ` with an empty diagnostic for a `.lgt` whose load halts
    (`lgt_file.cpp:356`).
  - A gate or case condition that halts counts as false on both sides (`lgt_cond.cpp`).
  - `lgt_load_tester` goes on after a neighbour consult halts (`lgt_io.cpp:672`).

## Files to split before their next edit

Near the 800-line limit (`wc -l`, 2026-11-18):

- `tests/tests.cmake` (778)
- `src/machine/internal.hpp` (729)
- `tests/cpp/session_test.cpp` (796)
- `tests/cpp/stream_test.cpp` (785)
- `tests/cpp/conformance_io_test.cpp` (781)
- `tests/cpp/machine_test.cpp` (779)

## Unsure

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
- Debugger: a goal whose context module is `system` gets no box, since that context means a goal a
  builtin runs for itself (findall/3's answer unification and the like). So `system:Goal` written
  by a user is not traced either.
- Debugger on Windows: `_isatty` is true for the NUL device (`src/platform/terminal.cpp:15`), so
  `mosa < NUL` is taken for a terminal and stops at the first port. The manual's recording line uses
  a pipe (`type nul | mosa ...`). Not run on Windows yet: the quoted `-DARGS` of the transcript tests.
- Debugger: `spy(nomod:foo/1)` raises `existence_error(module, nomod)` though the manual says a
  Spec may name an undefined predicate. A spy placed before and after a module loads can be listed
  twice. Notices (`% Spy point on ...`, abort) do not start on a fresh line after partial output.
- Debugger: the session-side abort branches (`-g` exit 1, init main, startup file, `:- if`
  roll-back, reconsult restore) and the mode roll-back are checked by hand, not by a C++ test.

## Not run yet

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

- `mosac build` of a source clause nested 100,000 deep works, but clang takes about 80 minutes
  and 1.2 GB on it (iso-notes 7.1).
- Grammar rules given to assertz/1 are translated only through `expand_term/2` of
  `library(dcg)`; `term_expansion/2` and `goal_expansion/2` do not exist.
- A Debug (/MTd) libmosa does not link with `mosac build` on MSVC.
- Undefined-call warnings show the clause line, not the goal line, and can name another file
  after a nested consult replaces that predicate's clauses (M23 A2).
- Library units are not traced under `--debug`.
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
  relative to the main file's directory for the program's own module files), which can differ
  from the interpreter's path.
- `mosac` still warns that an unqualified `maplist/3` after a goal `use_module(library(apply))`
  is undefined, though the executable runs (goal libraries are read as
  `use_module(library(N), [])`).
- A `M:G` into a module that no directive loads (one loaded only from a goal, or `lists` not
  loaded at all) goes through `:/2` at run time in a `mosac` executable.
- `mosac` does not bundle a file that a goal load names by a plain path, so the executable reads
  it from the working directory.

Phase 13: see CHANGELOG 0.4.0, Known limits.

## Tests

- `ctest --preset` runs in parallel (review item 5).
- On macOS runners `stack_deep_builtins` takes 250–510 s. Two push runs on ae22e43
  (43912835169, and the first attempt of 43912873254) went over the 600 s default `TIME_LIMIT`
  of `small_stack.cmake` at different items, while its PR run passed in 492 s and the rerun in
  466 s. Since 2026-11-17 (owner's A) that test alone gets 1200 s and a ctest `TIMEOUT` of 1500 s.
- Time ratios remain load-sensitive. index_scale's scan ratio failed once serially on a macOS
  runner (release run 42878532812, attempt 1).
- The slowest deep and scale tests run at `MOSA_GC_STRESS=1000`, or not under stress.

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
