Phase: 4
In progress: Module:Goal
Done: ADR 0006. module/2 and current_module/1. use_module/1 and use_module/2. library(Name) searches $MOSA_LIB, then lib/Name.pl beside the executable. library(lists) re-exports length/2 from system and defines the section 5 list predicates. A clause body runs in the defining module. ctest 31 passed. Version stays mosa 0.0.1 (phase 3)
Next: Module:Goal and existence_error(module, Module)
Open: interpretations left in iso-notes. Reconsult while a choice point exists still fails (session test). A second consult of the same file is still rejected. The design wants that reload to replace the file's clauses
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
