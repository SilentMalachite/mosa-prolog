English | [日本語](0026-goal-load-and-reexport.ja.md)

Status: Approved (2026-11-15, owner). SPEC section 9 gives phase 12 (module completion); this ADR fixes how a load called as a goal runs and what an export entry holds, so that `reexport/1,2` can export a predicate that another module defines.

## Context

`use_module/1,2` and `ensure_loaded/1` work only as directives; the goal `consult/1` (`run_consult` in `src/machine/consult.cpp`) already loads into the context module, as a nested load when it is called during a load. Phase 12 makes the three callable as goals, so how such a load meets the rollback (ADR 0017), the reconsult marks (ADR 0020) and `initialization/1` must be fixed first.
An export entry is a `PredKey` of the exporting module (`Module::exports`); an importer records that module in `Import::from`, and `resolve_pred` reaches the definition in one step.
`reexport/1,2` exports predicates that another module defines, some under an `as` alias, and the import conflict rule (iso-notes "Section 5 Conflict between two imports") compares import sources, so the shape of an entry decides what an importer of a re-exported predicate points at.

## Decision

A goal load is one outer load into the context module of the goal (`load_module` set to it, as `run_consult` does): the rollback of ADR 0017, the reconsult marks of ADR 0020 and the `initialization/1` goals apply as for a directive load, and a goal load called during a load is a nested load.
An export entry holds the name, the arity, the module that defines the predicate and its name there. `reexport` adds entries that name the defining module, so an importer's `Import::from` is the defining module and `resolve_pred` still reaches the definition in one step, however many re-exports lie between.
Two imports of one name that reach the same definition are not a conflict; a different predicate under that name is still `permission_error(import_into, procedure, From:N/A)`. `predicate_property(P, imported_from(M))` gives the defining module.

## Rejected alternatives

Following an import through the re-exporting module: `resolve_pred` would follow imports of imports, and the conflict check would compare the re-exporting module instead of the definition and report a conflict for one predicate reached along two paths.
Expanding a re-export again on the importing side: each import would read the source of the re-export again.

## Consequences

A failed goal load leaves no clauses, modules, imports or operators half made (ADR 0017), and writes its diagnostic to `user_error` (iso-notes question 1, A); the refusal to reload under a choice point (`take_over`, ADR 0017) stays (question 3, A).
The resolution rules of SPEC section 5 and ADR 0006 do not change, and a re-export adds no lookup step. Tasks 3 and 4 of the phase 12 plan implement the goal load and its diagnostic, and Task 6 the export entry and `reexport/1,2`.
Tests: the new rows of `tests/pl/modules.pl` (with files in `tests/pl/modules/`) and `tests/pl/japanese.pl`, the C++ session tests, and `mosatest --diff`.
