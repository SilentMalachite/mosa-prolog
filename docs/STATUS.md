# Status

- **Phase:** 0–13 done. Phase 14 (debugger) is on branch `phase14`; PR #4 is open.
- **Release:** `mosa 0.4.1`, released 2026-11-18 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** adversarial review of `main..HEAD` (focus `22846bdc..HEAD`), fixing what it finds.
- **Last done:** Muse review of `f55210cb..HEAD`: 3 Medium, 23 Low fixed (15 with tests); 10 left
  (open-items Doubts). ctest 235/235. Known box gaps: `lists:(G1, G2)`, `lists:findall/3`'s goal.
- **Next:** keep the commits after `5383fee4` local (owner B, 2026-11-19); when the owner asks,
  push them to PR #4, then CI on 3 OSes and GC-stress, then whether to close phase 14.
- **Questions to the owner:** A-or-B questions run on default A (open-items).
- **Split first:** 15 code and test files at 730 lines or more, listed in open-items.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
