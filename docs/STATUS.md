# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is open: SPEC section 9 row 14 and ADR 0028 exist.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** phase 14 Task 4 done, Task 5 not started. Phase 14 plan, 10 tasks:
  `docs/superpowers/plans/2026-11-18-phase14-debugger.md`.
- **Last done:** phase 14 Task 4: DebugState, the 9 predicates in `system`, flag `debug` link, Module::library.
- **Next:** plan Task 5: the box and the call/exit/redo/fail ports.
- **Questions to the owner:** four A-or-B questions in iso-notes run on default A (open-items).
- **Split first:** four tests (`internal.hpp` now 709, `write_pred.cpp` 704).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
