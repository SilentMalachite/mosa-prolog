# Status

- **Phase:** 0–14 done. Phase 14 (debugger) closed 2026-11-19; PR #4 merged into `main`.
- **Release:** `mosa 0.5.0`, released 2026-11-19 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2295/2333 (98.4%, test/1 counted: H-40), library column 600/605 (99.2%).
- **In progress:** none; W6 of the 2026-11-20 review on branch `fix/review-2026-11-20`;
  plan in [plans/2026-11-20-review-fixes.md](superpowers/plans/2026-11-20-review-fixes.md).
- **Last done:** W6 H-23 (a1a241e9: read/1 from a file pulls chunks only up to the first full stop),
  H-21, H-22; W5 to W2 done; W1 but H-39. W0 triage: [review-2026-11-20.md](review-2026-11-20.md).
- **Next:** H-24 (W6: a `portray/1` in another module makes print/1 and `~p` raise existence_error).
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** 9 code and test files at 730 lines or more, listed in open-items.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
