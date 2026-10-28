Phase: 4 in progress (phase 3 done)
In progress: none
Done: Logtalk suite pass rate is 90% or more. Version is mosa 0.0.1 (phase 3). Round-trip re-reading keeps the string
Done: mosatest --suite also writes docs/conformance.json (shields.io endpoint). The README badge reads it from main; the CI macOS job fails on an uncommitted diff
Done: the conformance suite moves the cwd to the scratch directory per tests.lgt, so {open(d1, write, S)} leaves no empty file in the repo root (conformance_cwd_test)
Done: mosatest --suite takes --report; it may be omitted only for tests/conformance from the repo root (suite_args_test)
Done: mosa no longer reads non-terminal stdin up front. --version returns on an open pipe and read/1 reads piped input (mosa_stdin)
Done: docs are English-primary with *.ja.md translations (README, AGENTS, SPEC, iso-notes, ADRs, bench); directory READMEs carry both languages
Done: phase 4 Task 1 (ADR 0006, 6 iso-notes answers) and Task 2: predicates keyed by (module, name, arity), goal frame word 1 carries the context module; all predicates still live in user. Suite 2028/2171 unchanged
Next: phase 4 Task 3, Module:Goal (src/module/qualify.cpp, tests/pl/modules.pl)
Open: interpretations left in iso-notes
Done: fixed SPEC typos in both languages: section 7 atom_length(X, foo) example, `char_conversion` in section 4, Japanese tests live in tests/pl/japanese.pl
Plan (phase 4): docs/superpowers/plans/2026-10-28-phase4-modules.md
Plan (phase 5, starts after phase 4 exit): docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md
