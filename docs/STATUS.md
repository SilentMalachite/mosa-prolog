Phase: 3 done
In progress: none
Done: Logtalk suite pass rate is 90% or more. Version is mosa 0.0.1 (phase 3). Round-trip re-reading keeps the string
Done: mosatest --suite also writes docs/conformance.json (shields.io endpoint). The README badge reads it from main; the CI macOS job fails on an uncommitted diff
Done: the conformance suite moves the cwd to the scratch directory per tests.lgt, so {open(d1, write, S)} leaves no empty file in the repo root (conformance_cwd_test)
Done: mosatest --suite takes --report; it may be omitted only for tests/conformance from the repo root (suite_args_test)
Done: mosa no longer reads non-terminal stdin up front. --version returns on an open pipe and read/1 reads piped input (mosa_stdin)
Done: docs are English-primary with *.ja.md translations (README, AGENTS, SPEC, iso-notes, ADRs, bench); directory READMEs carry both languages
Next: phase 4, modules
Open: interpretations left in iso-notes
Open: SPEC typos found while translating: the section 7 atom_length(X, foo) example contradicts itself; `charConversion` (section 4) vs the char_conversion flag; section 4 says tests/japanese/ but sections 8–9 say tests/pl/japanese.pl
Plan: docs/superpowers/plans/2026-10-26-phase3-exit.md
