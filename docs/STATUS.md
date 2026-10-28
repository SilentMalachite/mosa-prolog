Phase: 5 in progress (phase 4 done 2026-10-28: mosa 0.0.1 (phase 4), CI green on 3 OSes in run 40683841050)
Baseline (phase 5 start): 2041/2171 = 94.0%
In progress: none
Done (phase 5 Task 1): triage table of the 130 failing rows in the phase 5 plan (triage-table section): S 47, N 36, Q 31, I 15, X 1
Done (phase 5 Task 1): iso-notes A/B for the N groups (Section 3 if/3; Section 3 Flags dialect and version_data; Section 4 Stream encoding and BOM)
Done (phase 5 Task 1): new Q notes 9.3.10.3 (^)/2 and 8.11 Properties of the standard streams; So characters recorded as a SPEC difference
Change from the plan overview: the 5 `::/2` rows and lgt_open_4_31 call Logtalk library objects (os::, lgtunit::, integer::), so they are S (runner), not N
Note: Task 2 must also give the runner ignore/1, lgtunit variant/2, and the os::/lgtunit::/integer:: calls the suite uses
Note: suite dirs between_3, succ_2, plus_3, format_2, atomic_list_concat_2/3 are still in the side column (not run); lib tests cover them
Next: phase 5 Task 2, runner output capture (suppress_text_output/0, text_output_contents/1,2) then the six library imports
Open: Section 4 Stream encoding and BOM decided A (21 rows stay failing); other A/B questions stay in iso-notes for Task 8
Plan (phase 5): docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md
