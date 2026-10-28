Phase: 4 in progress (phase 3 done)
In progress: none (origin/main replaced by phase4-session at d729c99; old main kept as main-backup-2026-10-28)
Done: docs are English-primary with *.ja.md translations (README, AGENTS, SPEC, iso-notes, ADRs, bench); directory READMEs carry both languages
Done: phase 4 Task 1 (ADR 0006, 6 iso-notes answers) and Task 2: predicates keyed by (module, name, arity), goal frame word 1 carries the context module; all predicates still live in user. Suite 2028/2171 unchanged
Done: phase 4 Task 3: M:G (op 600 xfy, cut-transparent, call(M:G, A), culprit M:G). Task 4: module/2 (first term only), export/1, :- consult(F), exports imported into the loader's module, failed loads drop new modules. mod_not_first is in tests/cpp/module_load_test.cpp because load errors are diagnostics. Suite 2029/2171
Done: phase 4 Task 5: use_module/1,2 directive (as, except), loaded once, import conflicts and unexported imports fail the load, non-module files rejected, `as` is op(700, xfx). Error cases are in tests/cpp/module_load_test.cpp
Done: phase 4 Task 6 (CI green on 3 OSes, run 40650654198): use_module(library(N)) searches $MOSA_LIB then lib/ next to the executable (copied every build). Tests are in tests/cpp/library_path_test.cpp (run with and without MOSA_LIB), not modules.pl, since load errors are diagnostics and lib/ has no modules yet
Done: loader paths go through mosa::path_from_utf8/utf8_from_path, so Windows opens Japanese file names
Done: phase 4 Task 7: assert/retract/retractall/clause/abolish/current_predicate and dynamic/multifile take M:, clause heads M:H in files; ops follow own then explicit imports (src/module/target.cpp)
Done: phase 4 Task 8: current_module/1, predicate_property/2 (src/builtins/module_prop.cpp)
Done: phase 4 Task 9: meta_predicate/1 (op 1150 fx), meta args wrapped as Context:Arg on clause entry (src/module/meta.cpp), forall/2. maplist is a stand-in in tests/pl/modules/高階.pl until Task 13
Next: phase 4 Task 10, per-module operators
Open: interpretations left in iso-notes
Plan (phase 4): docs/superpowers/plans/2026-10-28-phase4-modules.md
Plan (phase 5, starts after phase 4 exit): docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md
