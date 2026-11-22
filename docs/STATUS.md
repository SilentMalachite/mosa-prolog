# Status

- **Phase:** 0–14 done. Phase 14 (debugger) closed 2026-11-19; PR #4 merged into `main`.
- **Release:** `mosa 0.5.0`, released 2026-11-19 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2295/2333 (98.4%, test/1 counted: H-40), library column 600/605 (99.2%).
- **In progress:** none; W8 of the 2026-11-20 review on branch `fix/review-2026-11-20`;
  plan in [plans/2026-11-20-review-fixes.md](superpowers/plans/2026-11-20-review-fixes.md).
- **Last done:** M-01 (4f2dc56e: a failed `AtomTable::intern` changes nothing); W7 (CI green);
  W6–W2; W1 but H-39. Triage: [review-2026-11-20.md](review-2026-11-20.md).
- **Next:** M-05 (W8: `0'c` and `0b`/`0o`/`0x` read byte by byte break on multibyte characters).
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** 8 code and test files at 730 lines or more, listed in open-items.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
