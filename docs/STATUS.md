Phase: 5 in progress (phase 4 done 2026-10-28: mosa 0.0.1 (phase 4), CI green on 3 OSes in run 40683841050)
Baseline (phase 5 start): 2041/2171 = 94.0%. Now: 2082/2171 = 95.9%
In progress: none
Done (phase 5 Task 1): triage-table section in the phase 5 plan; iso-notes A/B for the N groups; Section 4 Stream encoding and BOM decided A
Done (phase 5 Task 2): runner ^^suppress_text_output/0 and ^^text_output_contents/1,2,3 (src/machine/lgt_output.cpp, split from lgt_io.cpp); current input/output saved and restored per test like lgtunit run_test/2 (+19)
Done (phase 5 Task 2): runner imports lists, apply, between, format, strings, japanese into the test machine's user, except names the test file defines (src/machine/lgt_libs.cpp); mosa's user is unchanged (modules.pl between_needs_use) (+3)
New Q note: 7.10.5 a Writing a variable that occurs once (lgt_write_term_3_043/044/065/066 now fail only on `_`)
Runner limit: after a goal that leaves a choice point (e.g. stream_property/2), later plain goals run in a scratch machine (exec_plain), so their side effects on streams do not reach ^^ observers. No suite target has this shape yet
Decided A (2026-10-28): side-column dirs (between_3, succ_2, plus_3, format_2, format_3, atomic_list_concat_2/3) stay out through phase 5; denominator stays 2171
Done (phase 5 Task 3): =~= follows lgtunit 4decb64 (absolute 100*1e-12, then relative 0.00001; lists element by element) in src/machine/lgt_approx.cpp, split from lgt_judge.cpp; suite_approx_test (+14)
Done (phase 5 Task 4): ground/1 (Cor.2 8.3.10) in src/builtins/types.cpp, explicit stack, no binding; types.pl 8 tests incl. depth 30000 (+5, incl. stream_property_2_29..31)
Done (phase 5 Task 5): no code change; write.pl 8 option tests pass on the existing checker (wt_opt_no_partial: nothing written before an option error). lgt_write_term_3_118..122 expect the later repeated option to win, against the 8.14 Write options decision; question A/B in iso-notes (+0)
Next: Task 6, discontiguous loading (7.4.2.3, 5 rows). Later: Task 2 follow-up rows marked "2 追補" (ignore/1, os::, lgtunit::, integer::, condition)
Open: other A/B questions stay in iso-notes for Task 8
Plan (phase 5): docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md
