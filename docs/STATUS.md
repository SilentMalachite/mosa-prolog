# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is open: SPEC section 9 row 14 and ADR 0028 exist.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** nothing (phase 14 Task 2 done). Phase 14 plan, 10 tasks:
  `docs/superpowers/plans/2026-11-18-phase14-debugger.md`.
- **Last done:** phase 14 Task 2: moved TranslationLog, Stream, with_output_to/2 out; no behaviour change.
- **Next:** plan Task 3: write_term max_depth/1.
- **Questions to the owner:** four A-or-B questions in iso-notes run on default A (open-items).
- **Split first:** four tests (`internal.hpp` now 707, `write_pred.cpp` 690).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
