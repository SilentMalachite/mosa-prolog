Phase: 4
In progress: none
Done: Modules, the six libraries, the section 5 example, and version mosa 0.0.1 (phase 4). consult of a module's file replaces that file's clauses, drops its old import slots, and removes an empty local slot so a new import can take the name. A failed reload restores clauses, exports, operators, imports, and an assert from a thrown initialization. A reconsult inside a later-failing text keeps that module's clauses. A module file whose initialization throws rolls the new module back and keeps a dependency already loaded. A second module/2, and another file that reuses the name, still raise permission_error(modify, module, Module).
Done: use_module resolves library(Name), then a loaded module atom, then the atom as a path and that path plus .pl. During a load the path is relative to that file. At the toplevel it is relative to the working directory. ctest passed.
Next: phase 5 in SPEC section 9, the conformance rate and Japanese. Do not start until asked
Open: interpretations left in iso-notes. Reconsult while a choice point exists still fails (session test). A dynamic or multifile directive list does not yet accept Module:PI. A use_module/2 import list does not accept op/3
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
