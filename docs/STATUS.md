Phase: 4
In progress: use_module/2 (tests/pl/modules.pl)
Done: ADR 0006. current_module/1. consult of module/2 does not import. A module/2 that is not first, a second module/2, or another file repeating a loaded name throws permission_error(modify, module, Module) and leaves the first module. use_module/1 loads a new file, or imports an already loaded file without appending clauses. use_module/2 imports a list, PI as Alias, or except/1. ctest 30 passed. Version stays mosa 0.0.1 (phase 3)
Next: import conflict permission_error(import_into, procedure, PI). The failed import adds no slot. A module loaded by that call stays
Open: interpretations left in iso-notes. Reconsult while a choice point exists still fails (session test). A second consult of the same file is still rejected. The design wants that reload to replace the file's clauses
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
