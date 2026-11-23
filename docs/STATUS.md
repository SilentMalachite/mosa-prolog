# Status

- **Phase:** 0–14 done. Phase 14 (debugger) closed 2026-11-19; PR #4 merged into `main`.
- **Release:** `mosa 0.5.0`, released 2026-11-19 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2295/2333 (98.4%, test/1 counted: H-40), library column 600/605 (99.2%).
- **In progress:** W9 L-24 (`database.cpp`, `module/import.cpp`), on branch `fix/review-2026-11-20`;
  plan in [plans/2026-11-20-review-fixes.md](superpowers/plans/2026-11-20-review-fixes.md).
- **Last done:** W9 L-21 (bc0dc80e) and L-22 (a kept `library(...)` load keeps its `user:op/3`
  changes, fb3238eb); both red first; ctest 282/282. Earlier W9 items: review file.
- **Next:** W9 by file (W8's open items are owner questions on default A); 0.5.1 after W9 (A).
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** none; no code or test file is at 730 lines or more (open-items).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
