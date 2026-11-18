# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is on branch `phase14`; PR #4 is open.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** none.
- **Last done:** second adversarial review of PR #4, all 10 findings; the first message no longer
  loads `lists` (owner B, ADR 0007 amendment 3). ctest 233/233. Two box gaps are known limits.
- **Next:** keep the commits after `5383fee4` local (owner B, 2026-11-19); when the owner asks,
  push them to PR #4, then CI on 3 OSes and GC-stress, then whether to close phase 14.
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** 17 code and test files at 730 lines or more, listed in open-items.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
