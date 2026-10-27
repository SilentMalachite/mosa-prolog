Phase: 4
In progress: use_module of a module that is already loaded (tests/pl/modules.pl)
Done: ADR 0006. current_module/1. consult of module/2 does not import. A module/2 that is not first, a second module/2, or another file repeating a loaded name throws permission_error(modify, module, Module) and leaves the first module. use_module/1 loads a new module file and imports only its exports. ctest 30 passed. Version stays mosa 0.0.1 (phase 3)
Next: use_module of an already loaded module, then use_module/2
Open: interpretations left in iso-notes. Reconsult while a choice point exists still fails (session test). A second consult or use_module of the same file is rejected with the other-file error. The design wants consult to replace that file's clauses, and use_module to import without appending
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
