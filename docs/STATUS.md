Phase: 4
In progress: use_module/1 (tests/pl/modules.pl)
Done: ADR 0006. current_module/1. consult of module/2 does not import. module/2 that is not the first term, a second module/2, and another file that repeats a loaded name throw permission_error(modify, module, Module). The first module stays. ctest 30 passed. Version stays mosa 0.0.1 (phase 3)
Next: use_module/1
Open: interpretations left in iso-notes. Reconsult while a choice point exists still fails (session test). A second consult of the same file is rejected with the other-file error; the design wants that reload to replace clauses
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
