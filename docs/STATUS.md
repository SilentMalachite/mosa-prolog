# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is on branch `phase14`, pushed; PR #4 is open.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** none.
- **Last done:** adversarial review of PR #4, findings 1–3 (owner A): `a` runs the cleanups,
  `Lib:G` gets a box, tracing is linear (`debugger_scale`). ctest 228/228. The Lows are in open-items.
- **Next:** push the 3 fixes to PR #4 (not pushed yet), CI on 3 OSes and GC-stress, then whether
  to close phase 14.
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** six files (`tests.cmake` 784, `internal.hpp` 732, four C++ tests).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
