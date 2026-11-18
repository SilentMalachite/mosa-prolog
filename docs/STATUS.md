# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is open: SPEC section 9 row 14 and ADR 0028 exist.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** phase 14 Task 8 done, Task 9 not started. Phase 14 plan, 10 tasks:
  `docs/superpowers/plans/2026-11-18-phase14-debugger.md`.
- **Last done:** phase 14 Task 8: port commands g w + - h ?, unknown input, end of input as `n`.
- **Next:** plan Task 9: manual chapter 15 (`docs/manual/{en,ja}/15-debugger.md`).
- **Questions to the owner:** five A-or-B questions run on default A (open-items).
- **Split first:** four tests (`internal.hpp` now 729, `write_pred.cpp` 710, `tests.cmake` 778).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
