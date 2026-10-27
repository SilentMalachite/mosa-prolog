Phase: 4
In progress: library search and library(lists)
Done: ADR 0006. module/2 and current_module/1. use_module/1 and use_module/2. library(Name) loads lib/Name.pl beside the executable. A missing library fails like consult. ctest 30 passed. Version stays mosa 0.0.1 (phase 3)
Next: $MOSA_LIB is searched before lib/ beside the executable. library(lists) re-exports length/2. member/2 then succeeds
Open: interpretations left in iso-notes. Reconsult while a choice point exists still fails (session test). A second consult of the same file is still rejected. The design wants that reload to replace the file's clauses
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
