# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is on branch `phase14`, pushed; PR #4 is open.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** adversarial review of PR #4, owner A: `a` runs cleanups; `Lib:G` gets a box;
  the box depth is no longer counted per port.
- **Last done:** review fixes for PR #4: owner B/A/A (leash rollback, nested abort, depth over
  cyclic, positive Big in `max_depth`), then the 5 Minors (library mark reset, `StopKind::Abort`
  in CHANGELOG, exception-port `f`/`r` cleanup tests). ctest 227/227.
- **Next:** CI on 3 OSes and GC-stress for PR #4, then the question whether to close phase 14.
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** six files (`tests.cmake` 778, `internal.hpp` 729, four C++ tests).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
