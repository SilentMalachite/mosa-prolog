# Status

- **Phase:** 0–13 done. Phase 13 closed 2026-11-17 and is in main; SPEC section 9 has no row 14.
- **Release:** `mosa 0.4.0`, released 2026-11-17 (latest). Earlier releases are in CHANGELOG.
- **Conformance:** ISO column 2291/2326 (98.5%), library column 600/605 (99.2%).
- **In progress:** none.
- **Last done:** a cycle from an `if/3` argument back to that `if/3` raises
  `resource_error(cyclic_term)` (owner's A, 2026-11-17, iso-notes 7.3.3).
  Pushed 2026-11-18; CI run 44111341448 on d5f8581 passed all 11 jobs.
- **Next:** ask the owner for the next work.
- **Questions to the owner:** four A-or-B questions in iso-notes run on default A (open-items).
- **Split first:** `src/builtins/write_pred.cpp` (799 lines) and four test files near 800 lines.
- **Open items:** unsure points, decisions in force, known limits and doubts are in
  [open-items.md](open-items.md). Write new ones there, not here.
- **History:** `git log` (also for older forms of this file), CHANGELOG, ADRs, [bench.md](bench.md).
