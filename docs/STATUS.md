# Status

- **Phase:** 0–14 done. Phase 14 (debugger) closed 2026-11-19; PR #4 merged into `main`.
- **Release:** `mosa 0.5.0`, released 2026-11-19 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2295/2333 (98.4%, test/1 counted: H-40), library column 600/605 (99.2%).
- **In progress:** none; W7 of the 2026-11-20 review on branch `fix/review-2026-11-20`;
  plan in [plans/2026-11-20-review-fixes.md](superpowers/plans/2026-11-20-review-fixes.md).
- **Last done:** H-30 (b4907bf7, ADR 0011 amendment 5: deep bodies emit on explicit stacks); H-29;
  H-28; H-27; W6–W2; W1 but H-39. Triage: [review-2026-11-20.md](review-2026-11-20.md).
- **Next:** H-31 (W7: an atom longer than 64 KiB becomes one C++ string literal).
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** 8 code and test files at 730 lines or more, listed in open-items.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
