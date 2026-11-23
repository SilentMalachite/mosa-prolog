# Status

- **Phase:** 0–14 done. Phase 14 (debugger) closed 2026-11-19; PR #4 merged into `main`.
- **Release:** `mosa 0.5.0`, released 2026-11-19 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2295/2333 (98.4%, test/1 counted: H-40), library column 600/605 (99.2%).
- **In progress:** W9 `src/read/lexer_literal.cpp` (L-09), on branch `fix/review-2026-11-20`;
  plan in [plans/2026-11-20-review-fixes.md](superpowers/plans/2026-11-20-review-fixes.md).
- **Last done:** W9 `lexer.cpp`: L-10 (CR LF's LF goes through char_conversion) and L-13 (BOM
  only at a stream's start), 3a04bb2c, both red first; ctest 282/282.
- **Next:** W9 by file (W8's open items are owner questions on default A); 0.5.1 after W9 (A).
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** none; no code or test file is at 730 lines or more (open-items).
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
