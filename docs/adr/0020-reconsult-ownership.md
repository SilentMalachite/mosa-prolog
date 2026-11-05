English | [日本語](0020-reconsult-ownership.ja.md)

Status: Approved (2026-11-05; the owner chose A for review L11, which counts as approval)

## Context

Consulting a path again replaced only the predicates the new contents define. Predicates that the earlier load of that path defined and the new contents no longer have stayed behind: p/1, and d/1 declared there with `:- dynamic` (review L11). SPEC did not define reconsult, and the owner chose A in iso-notes ("7.4.2 Consulting a path again"): remove them, as SWI-Prolog does.
A load can be undone after it has read its text. A failed directive in the file that consulted it undoes it (ADR 0017), so a removal done at the end of a nested load would have to be undone with it, and the snapshots can only restore a predicate whose slot still exists.

## Decision

Each predicate records the file whose text last defined it (`Predicate::source_file`, an index into `Impl::source_files`, which holds canonical keys). A text defines a predicate when it gives it clauses or declares it dynamic; the load notes the predicate, its current definition (`defined_at`) and the file being read (`Impl::load_owner`) at each dynamic declaration and at the start of each run of clauses, so another file that declares it again or adds clauses to it in the same load becomes the owner. A load of text with no file notes no file, and a predicate it redefines belongs to no file. Included text counts for the file that includes it. A file read by ensure_loaded or use_module owns its predicates itself, so a reconsult that skips the prepared file leaves them alone.
When a file's text has been read to the end, the load notes the file (`FileNotes` in `LoadSnapshots`). A nested load hands its notes up to the load that read it, as it hands up its snapshots, and a failed load drops them; a load whose initialization goal raised keeps its text (`load_kept`), so it keeps its notes too. Only the outermost load applies the notes, before its initialization goals and once more after them for the files those goals consult: a predicate whose source file is a noted file and that no text of this load defined is abolished as abolish/1 would do it, even when it is static, with its generated code detached and all such predicates taken out of the table in one pass; then each noted predicate whose definition is still the one the text made takes the file as its source, in the order of the notes.
Predicates made by goals have no source file and stay, and a goal that begins a definition (assertz after abolish) clears the source file. A multifile predicate stays, since other files may have added clauses to it. A predicate that another file defined later belongs to that file and stays. A predicate that the new text only declares dynamic, or that a goal of this load asserted into or retracted from (ADR 0017 keeps what goals did), stays but loses the clauses it had before the load, since a declaration does not replace clauses as a first clause does; the result is the same as reading the new contents for the first time.

## Rejected alternatives

Remove the stale predicates at the end of each load, nested or not. An enclosing load that fails afterwards cannot bring them back: the snapshots restore slots, and abolish erases a slot that has no open scan.
A map from file to its predicates. When another file takes over a predicate, every other file's list has to be searched and updated; the field on the predicate costs four bytes, which fit in existing padding.
Wipe everything the file defined at the start of the reload, as SWI-Prolog does. Initialization goals and directives of the new text would see an empty program, and a load that fails partway would have to rebuild the old definitions.

## Consequences

Editing a file and consulting it again leaves no stale predicates; a reconsult that fails keeps the old ones. A nested consult's stale predicates are removed only when the outermost load succeeds, so the initialization goals of a nested file still see them. Clauses that goals added before the reload to a removed dynamic predicate go with it. A reconsult whose initialization goal raises keeps the new text, as before, and removes the stale predicates. Ownership is per predicate, not per clause: a multifile predicate keeps the clauses of a file that no longer gives them, and gains them again each time that file is read. A file read twice within one outer load counts as one read, so a predicate that only the first read defined stays.
Tests: tests/cpp/reconsult_test.cpp.
