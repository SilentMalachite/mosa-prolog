# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is implemented on branch `phase14` (not pushed).
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** none. Plan: `docs/superpowers/plans/2026-11-18-phase14-debugger.md`.
- **Last done:** phase 14 Task 10 (CHANGELOG, README, iso-notes, SPEC, ADR 0028, bench), then the
  final-review fixes (header comments, duplicate spy keys, no-port goal list, write.pl row, notes).
- **Next:** owner OK to push `phase14` and run CI on 3 OSes and GC-stress (pending); then the question
  whether to close phase 14.
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** six files (`tests.cmake` 778, `internal.hpp` 729, four C++ tests).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
