Phase: 4 in progress (phase 3 done)
In progress: phase 4 Task 6 Step 3, waiting for the 3-OS CI run (main is not pushed; Windows reads Japanese paths for the first time)
Done: Logtalk suite pass rate is 90% or more. Version is mosa 0.0.1 (phase 3). Round-trip re-reading keeps the string
Done: mosatest --suite also writes docs/conformance.json (shields.io endpoint). The README badge reads it from main; the CI macOS job fails on an uncommitted diff
Done: the conformance suite moves the cwd to the scratch directory per tests.lgt, so {open(d1, write, S)} leaves no empty file in the repo root (conformance_cwd_test)
Done: mosatest --suite takes --report; it may be omitted only for tests/conformance from the repo root (suite_args_test)
Done: docs are English-primary with *.ja.md translations (README, AGENTS, SPEC, iso-notes, ADRs, bench); directory READMEs carry both languages
Done: phase 4 Task 1 (ADR 0006, 6 iso-notes answers) and Task 2: predicates keyed by (module, name, arity), goal frame word 1 carries the context module; all predicates still live in user. Suite 2028/2171 unchanged
Done: phase 4 Task 3: M:G (op 600 xfy, cut-transparent, call(M:G, A), culprit M:G). Task 4: module/2 (first term only), export/1, :- consult(F), exports imported into the loader's module, failed loads drop new modules. mod_not_first is in tests/cpp/module_load_test.cpp because load errors are diagnostics. Suite 2029/2171
Done: phase 4 Task 5: use_module/1,2 directive (as, except), loaded once, import conflicts and unexported imports fail the load, non-module files rejected, `as` is op(700, xfx). Error cases are in tests/cpp/module_load_test.cpp
Done: phase 4 Task 6 Steps 1-2: use_module(library(N)) searches $MOSA_LIB then lib/ next to the executable (copied every build). Tests are in tests/cpp/library_path_test.cpp (run with and without MOSA_LIB), not modules.pl, since load errors are diagnostics and lib/ has no modules yet
Next: push, check CI on 3 OSes, then phase 4 Task 7, M:-qualified clause operations and declarations
Open: interpretations left in iso-notes
Plan (phase 4): docs/superpowers/plans/2026-10-28-phase4-modules.md
Plan (phase 5, starts after phase 4 exit): docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md
