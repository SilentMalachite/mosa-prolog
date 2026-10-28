Phase: 3 done
In progress: phase 4 Task 1. ADR 0006 approved (2026-10-28). Waiting for answers to 2 iso-notes questions (8.4.3 sort/2, section 7 forall/2), one per report. No phase 4 code until then
Done: Logtalk suite pass rate is 90% or more. Version is mosa 0.0.1 (phase 3). Round-trip re-reading keeps the string
Done: mosatest --suite also writes docs/conformance.json (shields.io endpoint). The README badge reads it from main; the CI macOS job fails on an uncommitted diff
Done: the conformance suite moves the cwd to the scratch directory per tests.lgt, so {open(d1, write, S)} leaves no empty file in the repo root (conformance_cwd_test)
Done: mosatest --suite takes --report; it may be omitted only for tests/conformance from the repo root (suite_args_test)
Done: mosa no longer reads non-terminal stdin up front. --version returns on an open pipe and read/1 reads piped input (mosa_stdin)
Done: docs are English-primary with *.ja.md translations (README, AGENTS, SPEC, iso-notes, ADRs, bench); directory READMEs carry both languages
Next: phase 4, modules
Open: interpretations left in iso-notes
Done: fixed SPEC typos in both languages: section 7 atom_length(X, foo) example, `char_conversion` in section 4, Japanese tests live in tests/pl/japanese.pl
Plan (phase 4, next): docs/superpowers/plans/2026-10-28-phase4-modules.md. Task 1 (ADR 0006 and 6 iso-notes questions) needs owner approval before any code
Plan (phase 5, starts after phase 4 exit): docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md
