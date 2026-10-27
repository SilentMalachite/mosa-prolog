Phase: 4
In progress: none
Done: ADR 0006. module/2 and current_module/1. use_module/1 and use_module/2. library(Name) searches $MOSA_LIB, then lib/Name.pl beside the executable. library(lists) re-exports length/2 from system and defines the section 5 list predicates. A clause body runs in the defining module. Module:Goal runs in the named module. meta_predicate/1 wraps 0, :, ^, and // with the calling module. library(apply) exports maplist/2 through maplist/5, foldl/4 through foldl/6, include/3, and exclude/3. A directive use_module imports into the module being loaded. An atom that names a loaded module selects that module. The morning session's 確認 succeeds. ctest 31 passed. Version stays mosa 0.0.1 (phase 3)
Next: database predicates honor the calling module and a qualified head
Open: interpretations left in iso-notes. Reconsult while a choice point exists still fails (session test). A second consult of the same file is still rejected. The design wants that reload to replace the file's clauses. An atom that is not a loaded module is not yet tried as that path plus .pl
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
