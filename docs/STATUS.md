# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is on branch `phase14`; PR #4 is open.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** none.
- **Last done:** adversarial review of `main..HEAD` (2 Medium, 9 Low fixed; older doubts in
  open-items); pushed to PR #4 at `9083f681`, CI green in all jobs. ctest 237/237.
- **Next:** the owner decides whether to close phase 14. Known box gaps: `lists:(G1, G2)`,
  `lists:findall/3`'s goal. CI timing notes (transcript limit, ASan) are in open-items Tests.
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** 15 code and test files at 730 lines or more, listed in open-items.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
