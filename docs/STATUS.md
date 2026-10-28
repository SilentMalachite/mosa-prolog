Phase: 4
In progress: none
Done: Modules, the six libraries, the section 5 example, and version mosa 0.0.1 (phase 4). consult of a module file replaces that file's clauses. A failed reload restores clauses, exports, operators, and imports. use_module resolves library(Name), then a loaded module atom, then a path.
Done: 7.7.7 b is B. A predicate the caller's resolution order does not find raises existence_error(procedure, PI) when unknown=error, even if another module defines that name and arity. unknown=fail still fails. An empty dynamic predicate that the search found still fails. ctest 32 passed.
Next: phase 5 in SPEC section 9, the conformance rate and Japanese. Do not start until asked
Open: length/2 wording in iso-notes. Reconsult while a choice point exists still fails (session test). A dynamic or multifile directive list does not yet accept Module:PI. A use_module/2 import list does not accept op/3
Open: errors inside a nested :- use_module are lost. Bad import lists and a duplicate alias fail silently. A failed load with meta_predicate leaves an empty predicate. A reload keeps a predicate dynamic after its dynamic line is removed. op/3 outside user is not undone on backtracking. format is O(n^2) in literal text. length(L, L) does not terminate. predicate_property ignores the user fallback and built-ins
Plan: docs/superpowers/specs/2026-10-27-phase4-modules-design.md
