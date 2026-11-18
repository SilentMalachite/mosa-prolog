# Status

- **Phase:** 0–13 done. Phase 13 closed 2026-11-17 and is in main; SPEC section 9 has no row 14.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** phase 14 (debugger) plan, awaiting the owner's review (9 tasks):
  `docs/superpowers/plans/2026-11-18-phase14-debugger.md`. Design approved; questions 1 and 2 are A.
- **Last done:** release 0.4.1 (owner's request): tag `v0.4.1` at e6ddf1b, published as latest;
  the landing page on `gh-pages` points at 0.4.1 (e322068).
- **Next:** plan Task 1 opens the phase (SPEC row 14, iso-notes, ADR 0028), docs only.
- **Questions to the owner:** four A-or-B questions in iso-notes run on default A (open-items).
- **Split first:** `src/builtins/write_pred.cpp` (799 lines) and four test files near 800 lines.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
