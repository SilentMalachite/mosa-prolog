English | [日本語](iso-notes.ja.md)

# ISO Interpretation Notes

When the standard and the spec disagree, write one "A or B?" question with the clause number. Do not implement before confirmation.

## Spec Section 4: code points of 𠮷野家

Decision (2026-10-25): Count Unicode code points (scalar values). `𠮷野家` is three: U+20BB7, U+91CE, U+5BB6. `atom_length('𠮷野家', 3)` is correct. It is not the four UTF-16 code units. The single character 𠮷 gives `atom_length('𠮷', 1)` and `char_code('𠮷', 134071)`.

## 8.3.9 callable/1

Decision (2026-10-25): B. As in 8.3.9.3 of Corrigendum 2, Errors is None, and `callable(3)` and `callable(X)` fail.
`call/1` still throws `instantiation_error` for a variable and `type_error(callable, Value)` for a number.

## 8.16 atom_number/2 (non-ISO)

Decision (2026-10-25): Open question 2: A. Register `atom_number/2` and convert in both directions. If both arguments are variables, `instantiation_error`. Atom to number uses the same parsing as `number_chars`; text that does not match gives `syntax_error(Reason)`. Number to atom gives the text of unquoted `write_term`. If both are instantiated, succeed if they match and fail otherwise.

## 8.10 findall/4 (non-ISO)

Decision (2026-10-25): `findall(Template, Goal, List, Tail)` is a difference list. If there are no solutions, List is unified with Tail; Tail is not bound to `[]`. The fourth argument is not checked as a list. `findall/3` is `findall/4` with the tail `[]`.

Decision (2026-10-27): The "not checked" above is withdrawn. If the fourth argument is neither a partial list nor a list, throw `type_error(list, Tail)` before collecting. A variable tail is bound at the end of the solutions. An empty list and a proper list also remain the tail. The tail is not bound before the check finishes.

## 8.14 print/1 and print/2

Decision (2026-10-25): Open question 4: B. `print/1` and `print/2` do not call `portray/1` and output the same text as `write/1`. Registered under term output.

Decision (2026-10-27): The B above is withdrawn. When `portray/1` has clauses and the goal succeeds, `print/1` and `print/2` use its output as the text of the term. When it has no clauses, the procedure is the same as `write/1`. When it has clauses but fails, that term gets the same text as `write/1`, and only the arguments of a compound term are passed through `print` again. `portray/1` is not made a built-in. `write/1` does not look at `portray`. Output written by a failed `portray` is not kept.

## 8.14 Write options

Decision (2026-10-25): `write/1,2` and `print/1,2` use `numbervars(true)`. `writeq/1,2` uses `quoted(true)` and `numbervars(true)`. `write_canonical/1,2` uses `quoted(true)` and `ignore_ops(true)`. In `write_term` all three default to false. When values of the same boolean option conflict, `domain_error(write_option, LaterTerm)`. `variable_names` is taken once; if a name is not a variable spelling, `domain_error(write_option, ThatPair)`. A Var that is already bound is ignored.

## 8.14.3 op/3

Decision (2026-10-26): The goal calls `OperatorTable::define`. Checks run in the order instantiation, type, domain, permission; for a list of names, every name is checked before the table is replaced. On rejection the table is not changed. A nonzero declaration replaces the old specifier of the same class (prefix, infix, postfix). Priority 0 removes only that specifier. Priority 0 for `|` succeeds as a removal, as in the Cor.2 example. Infix with priority from 1 to 1000 inclusive, as well as prefix and postfix, give `permission_error(create, operator, '|')`. Comma gives `permission_error(modify, operator, ',')`. `[]` and `{}` give `permission_error(create, operator, ThatAtom)`. The `:- op` directive uses the same `define`.

## 8.14.4 current_op/3

Decision (2026-10-26): Checks follow argument order, and the first one that applies is thrown. If the priority is not a variable and not an integer from 0 to 1200 inclusive, `domain_error(operator_priority, Priority)`. Atoms and floats also get this domain error. If the specifier is not a variable and not one of `xf` `yf` `xfx` `xfy` `yfx` `fx` `fy`, `domain_error(operator_specifier, Op_specifier)`. Integers and compound terms also get this domain error. If the operator is neither a variable nor an atom, `type_error(atom, Operator)`. Priority 0 is in the domain. If nothing matches, the goal fails. Candidates are copied at call time; even if `op/3` changes the table during backtracking, the copied candidates remain. When the name is an atom, the order is the same as `lookup`: ascending priority, and within the same priority, specifier declaration order. When the name is a variable, the order is table order.

## 8 with_output_to/2

Decision (2026-10-25): Only the first solution is taken. On success, `atom/1` and `codes/1` are bound. On failure and exception, nothing is bound. The current output is restored on success, failure, and exception alike. The goal runs in the current context. The memory stream it creates is closed when done (see "8 with_output_to memory" below).

## 8.8 clause/2

Decision (2026-10-25): Open question 5: A. User-defined predicates are public. This includes static clauses from `consult`, `:- dynamic`, predicates created by `assert`, and dynamic predicates whose clauses were all removed by `retract`. `clause/2` returns clauses in definition order. The body of a fact is the atom `true`. Variables in the same slot are the same cell; an anonymous variable is a fresh reference at each occurrence. `false/0` in a body is folded to `fail` by the compiler, so the returned body is the atom `fail`. Built-ins and control constructs give `permission_error(access, private_procedure, PI)`. A nonexistent predicate fails. A dynamic predicate with no clauses fails but is not private. Static user predicates can be read.

## 8.8 current_predicate/1

Decision (2026-10-25): Open question 6: B. `current_predicate/1` returns only user predicates, in definition order. `current_predicate(true/0)` fails; it is not a `permission_error`. An empty dynamic predicate succeeds. After `abolish` it fails.

## 8 Memory streams for I/O tests

Decision (2026-10-25): Open question 7: A. The names are `open_string/2` and `with_output_to/2`. Location and newlines are in ADR 0005.

## 7.11 Flag char_conversion

Decision (2026-10-25): Open question 8: A. The default of the flag `char_conversion` is `off`. When `on`, reading uses the mapping. The initial mapping stays the identity.

## 8.14.5 char_conversion/2

Decision (2026-10-26): Checks run in the order: In is a variable, In is not a one-character atom, Out is a variable, Out is not a one-character atom. The type error is `type_error(character, ThatArgument)`. Mapping a character to itself removes the mapping for that In and returns it to the identity. Mapping to a different character replaces it; there is only one mapping per In. If `a` is mapped to `b` and `b` to `c`, `a` still maps to `b`. While the flag is `off`, reading does not use the mapping. When `on`, only code points outside quotes and outside comments are mapped. Inside quotes and comments the original characters remain. The character after `0'` is outside quotes, so it is mapped. The directive `:- char_conversion` uses the same checks, and if the load fails, the flag and the mapping are restored. The values are `on` and `off`.

## 8.14.6 current_char_conversion/2

Decision (2026-10-26): Checks run on In, then Out. Variables are allowed. If an argument is not a one-character atom, `type_error(character, ThatArgument)`. No instantiation error is thrown. Identity pairs are not solutions. Initially there are no solutions. Giving the same variable as both arguments also fails. The plan's "in the identity case, it binds to itself" is not adopted. The flag being on or off does not affect queries. Even when the flag is off, registered pairs are returned, and reading does not map. Candidates are a copy taken at call time, in registration order. Replacing the mapping for the same In moves that pair to the end. Even if a mapping is removed during backtracking, the copied candidates remain. The pairs from mapping `a` to `b` and `b` to `c` are two solutions, unchanged.

## 7.4.2 discontiguous/1

Decision (2026-10-26): Directive only. The argument is a predicate indicator or a list of them, as with `dynamic/1`. A comma-separated sequence is not an indicator. Within the same text, only predicates declared before the gap may have discontiguous clauses. Without a declaration, the diagnostic remains `述語 名前/アリティ の節が離れている` ("clauses of predicate Name/Arity are discontiguous"). The declaration does not create a procedure. If there are no clauses, `current_predicate/1` fails and a call gives `existence_error`. The declaration does not carry over to the next load. The next `consult` replaces the predicate. Gaps in a predicate with a different arity or a different name are not allowed.

## 7.4.2 multifile/1

Decision (2026-10-26): Directive only. The argument is a predicate indicator or a list of them, as with `dynamic/1`. A comma-separated sequence is not an indicator. `Module:PI` is not written until Phase 4. The declaration does not create a procedure. If there are no clauses, `current_predicate/1` fails and a call gives `existence_error`. The declaration carries over to the next load. For a predicate that carried over, the next load adds clauses and does not replace them. Loading the same text twice doubles the clauses. A predicate without the declaration is replaced, and its solutions are not doubled. A declaration that comes after the first clause in this load does not undo the replacement made by this load. That declaration remains, and the load after that adds clauses. Clauses are not added to a different arity or a different predicate. A failed load undoes the clauses and declarations it added. A predicate that is still dynamic stays dynamic after clauses are added. Gaps are not allowed, and the diagnostic remains `述語 名前/アリティ の節が離れている`.

## 7.4.2 initialization/1

Decision (2026-10-26): A. Clauses are kept even on exception. Directive only. The goal runs once, in source order, after the text is prepared. If the argument is a variable, `instantiation_error`; if it is not callable, `type_error(callable, Goal)`. These two make the load fail and undo the clauses, and the goal is not run. Failure is not an error: the load succeeds, the clauses remain, and later initialization goals also run. Side effects that happened before the failure remain. An uncaught exception makes the load fail with a diagnostic that is its Formal if it is `error/2`, and otherwise the thrown term. Later initialization goals are not run. The clauses, and side effects before the exception, are kept.

## 7.4.2 include/1

Decision (2026-10-26): Directive only. The clauses enter the same text at the position of the directive. The diagnostic for discontiguous clauses remains `述語 名前/アリティ の節が離れている`. `op`, `discontiguous`, and `initialization` in the included file also take effect in the same text at that position. Initialization goals run in source order after the whole text is prepared. A relative path is opened from the directory of the file that contains the directive. A text-only load opens the path as given. Including the same file twice doubles the clauses. Opening a file that is already open, recursively, gives the diagnostic `パス: ファイルが循環しています` ("Path: file cycle detected"), and the clauses are undone. If the argument is a variable, `instantiation_error`; if it is not an atom, `type_error(atom, File)`. These two make the load fail and undo the clauses. `[]` is a file name, not an empty directive. If the file cannot be opened, the load fails with `パス: ファイルを開けません` ("Path: cannot open file") and the clauses are undone. The standard's `existence_error(source_sink, File)` is not used. A syntax error in a nested file keeps that file's path and position in the diagnostic. For a name without an extension, if that path is not a regular file, `.pl` is appended to the same name and that is opened. Nothing is appended when the name opens as is or already has an extension. If neither exists, the path in the diagnostic is the name as given.

## 7.4.2 ensure_loaded/1

Decision (2026-10-26): Directive only. The argument, the diagnostic when the file cannot be opened, and extension handling are the same as include/1. A variable gives `instantiation_error`; a non-atom gives `type_error(atom, File)`. `[]` is a file name. If the file cannot be opened, the load fails with `パス: ファイルを開けません` and the clauses are undone. The standard's `existence_error(source_sink, File)` is not used. The clauses enter the same text at the position of the directive. Calling `ensure_loaded` twice on the same canonical path adds neither clauses nor initialization. A relative path and an absolute path to the same file count once. Files that were `include`d, files that were `consult`ed, and the file currently open are also already prepared. A later `ensure_loaded` does not reload them. `include` adds clauses even if the file is already prepared. `consult` reloads and replaces. A file that `ensure_loaded`s itself, or files that `ensure_loaded` each other, are not treated as a cycle. A failure that undoes clauses also undoes the prepared paths. When the load fails due to an initialization exception, the clauses remain, so the path also remains, and the next `ensure_loaded` does not rerun the exception.

## 7.4.2 [File]

Decision (2026-10-26): Directive only. When the body is a list, each atom is loaded from left to right by the same procedure as `consult`. An empty list does nothing. An element `[]` is a file name. A partial list and a variable element give `instantiation_error`. If there is even one variable, this takes precedence over the type check. If an element is not an atom, `type_error(atom, ThatElement)`. If it is not a closed list, `type_error(list, ThatTerm)`. `:- X.` is not a list. A relative path is opened from the directory of the file that contains the directive. A text-only load opens the path as given. If a file cannot be opened, the load fails with `パス: ファイルを開けません` and the clauses are undone. The clauses do not continue the parent's text. Loading another predicate does not make the parent's clauses discontiguous. `consult` replaces predicates, and loading the same path again runs initialization again. A later `ensure_loaded` does not reload it. For predicates the parent has already seen, later clauses are added. Predicates the parent has not yet seen are replaced by later clauses. Opening the file itself fails with `ファイルが循環しています` and the clauses are undone. An initialization exception keeps the clauses, and later clauses and initializations are not read. If any other failure occurs partway through the list, clauses replaced before that point are also restored.

## 7.11 Flag debug

Decision (2026-10-25): Open question 9: A. The default is `off`. Even when set to `on`, Phase 3 does not trace.

## 7.11 Flag unknown: warning

Decision (2026-10-25): Open question 10: B. `unknown` accepts only `error` and `fail`. `warning` is not accepted.

## 7.11 Flags float_overflow, float_zero_div, float_undefined

Decision (2026-10-25): Open question 11: B. `float_overflow`, `float_zero_div`, and `float_undefined` accept only `error`. No other atom names are introduced.

## Section 4 Flag invalid_utf8

Decision (2026-10-25): Open question 12: A. The default of `invalid_utf8` is `error`. With `replace`, U+FFFD is inserted.

## Section 4 Flag japanese_punctuation

Decision (2026-10-25): Open question 13: A. `set_prolog_flag(japanese_punctuation, true)` stores `true` as the value. Lexical analysis is not changed.

## 7.11 Flag version

Decision (2026-10-25): Open question 14: A. `version` is a read-only atom, and its text is the same as `version_line()`.

## Section 3 Flag iso

Decision (2026-10-25): Open question 15: A. The default of `iso` is `true`, and it is read-only. Phase 3 adds no new behavior.

## 8.12 Newlines in text

Decision (2026-10-25): Open question 16: A. In text files and memory streams alike, on every OS, a newline is a single U+000A. It is not left to C++ text mode.

## 8.11 Stream terms

Decision (2026-10-25): A stream term is always `'$stream'(N)`. `N` is the index into the stream vector. An alias is a separate name and does not change the form of the term.

## 8.11 Standard streams

Decision (2026-10-25): Index 0 is `user_input` (stdin), 1 is `user_output` (stdout), 2 is `user_error` (stderr). `reposition` is false.

## 8.11 Reading past end of stream

Decision (2026-10-25): The default `eof_action` is `eof_code`. Reading past the end gives `end_of_file` for text and `-1` for binary. `eof_action(reset)` can be chosen separately.

## 8.11 Empty input

Decision (2026-10-25): When the position equals the length of the source, `end_of_stream` is `at`. Immediately after an empty input is opened, it is `at`.

## 8.11 Position term

Decision (2026-10-25): `stream_position(CharNo, Line, LinePos)`. The character count starts at 0; the line and the position within the line start at 1.

## 8.11.5.3 f Stream argument is instantiated

Decision (2026-10-25): B. As in 7.12.2 and 8.11.5.3 f of Cor.2, `uninstantiation_error(Stream)`. The `Stream` of `open_string/2` also has mode `-`, so it gets the same Formal. `uninstantiation_error/1` was added to the list in Spec Section 3.

## 7.10.2.11 eof_action(reset)

Decision (2026-10-26): A. Reading with `reset` after the end of stream moves the position back to the beginning and reads again. Test `reset rereads`. 7.10.2.11 only says to clear the past-end-of-stream mark and read again. A file is copied into a byte sequence when it is opened, so clearing only the mark leaves the position at the end, and the next read hits the end again, the same as `eof_code`. The approved plan is "read the file again", so the position goes back to the beginning. This is recorded as a deviation from 7.10.2.11.

## 7.8.3 call/1 and multifile/discontiguous predicates with no clauses

Logtalk's `lgt_call_1_17` and `lgt_call_1_18` expect `call/1` on the clause-less `fenix/1` (multifile) and `scattered/2` (discontiguous) to fail. Under the 7.4.2 decisions and `tests/pl/directives.pl`, a declaration alone does not create a procedure, and with `unknown=error` the result is `existence_error(procedure, PI)`. We read 7.4.2 as saying that only `dynamic/1` creates an empty procedure.

A or B?: A keeps 7.4.2 as is: `existence_error`. B: the directive alone creates a procedure, and if there are no clauses, `call/1` fails. B is not implemented before confirmation.

## 8.11.1.3 Domain of current_input

Decision (2026-10-26): A. Passing a term that is neither a stream nor an alias to `current_input/1`, `current_output/1`, or `stream_property/2` gives `domain_error(stream_or_alias, S)`. Tests `input_bad`, `output_bad`, `prop_bad_stream`. The alias `user_input` succeeds with `current_input(user_input)`. Because aliases are in the domain, the Formal is `stream_or_alias`, not `stream`. This is recorded as a deviation from `domain_error(stream, S)` in 8.11.1.3 a and 8.11.8.3 a. Predicates that already use `stream_or_alias`, such as `set_input/1`, are unchanged.

## 8 with_output_to memory

Decision (2026-10-26): A. The memory stream created for `atom/1` and `codes/1` is closed on success, failure, exception, and halt. On success, the bytes are bound before the stream is closed. A closed slot is not reused. Afterwards, `stream_property` does not pick it up when enumerating. Passing that term gives `existence_error(stream, S)`. A stream passed in by the caller is not closed. The stream stays open while the goal runs, so `current_output` and `stream_property` inside the goal can see it.

## Conformance suite

Decision (2026-10-26): We do not wait for permission. `inriasuite.pl` has no redistribution permission, so it is not bundled. The Phase 3 conformance suite is Logtalk's `tests/prolog`, placed at commit `4decb64b598feff542a926a085eef01ec51f5a72` as is under `tests/conformance/logtalk/prolog`. Its license is Apache-2.0, the same as the repository, and the copyright notices are kept. The files are not modified. The 90% in Spec Section 9 is measured with this suite. This decision does not state a pass rate.

## License

Decision (2026-10-26): The repository is unified under Apache-2.0. The text is in `LICENSE`. Mosa's copyright notice is `Copyright 2026 Mosa Prolog contributors`, placed in `NOTICE`. The Logtalk tests are under the same license, and their copyright stays as Paulo Moura's notice.

## 9.3.1.3 d (**)/2: negative power of 0

`lgt_power_2_14` expects `**(0, -1)` to give `evaluation_error(undefined)` or `evaluation_error(zero_divisor)`. ECLiPSe's IsoErrata says that the text of 9.3.1.3.d is undefined, and that it should be zero_divisor because infinity is defined. 9.3.10.3 d of Corrigendum 2 leaves the same form of `(^)/2` as undefined. Currently `std::pow(0, negative)` is infinity, so the result is `evaluation_error(float_overflow)`.

A or B?: A: `evaluation_error(undefined)`, following the text of 9.3.1.3.d. B: `evaluation_error(zero_divisor)`, following the errata. `float_overflow` is not changed before confirmation.

## 7.5.4 Open scans and retract

`logical_update_semantics_retract` expects that even if `retract` runs during a scan, the clauses present at call time are all tried to the end. The same applies to the same pattern with `retractall` and `abolish`. ADR 0004 decided that retract removes the clause from subsequent candidates and keeps the rest of the current clause. `retract_open_later` in `tests/pl/database.pl` is `[X = 1, X = 2]`; the removed `later(3)` does not appear.

A or B?: A keeps ADR 0004: removed clauses are dropped from open scans. B follows 7.5.4: the call tries the initial sequence. Stay with A until confirmed.

## 8.11.8 position with reposition(false)

`sics_set_stream_position_2_06` takes `stream_property(Stream, position(Position))` from a stream opened with `reposition(false)` and expects `set_stream_position(Stream, Position)` to give `permission_error(reposition, stream, Stream)`. With the file copied in, the actual result is `fail`. `stream_property/2` does not report `position/1` for a stream that cannot be repositioned ("no position" in `tests/cpp/stream_test.cpp`). `set_stream_position/2` already throws `permission_error(reposition, stream, Stream)` when given a position term for such a stream ("reposition formal" in the same file).

A or B?: A: do not report the property (the current implementation; the goal fails). B: report `position/1`, then give `permission_error`. A until confirmed. B is not implemented.

## memberchk/2 and iso_stream_property_2_01

`memberchk/2` belongs to `library(lists)` and is not made a built-in in Phase 3. The assertion of `iso_stream_property_2_01` runs the helper clauses in the test file as the user clauses present at that time. The reading that leaves the row as `fail` is withdrawn.

A or B?: A: do not add a built-in `memberchk/2`. B: add it as a built-in. A until confirmed. B is not implemented.

## 8.11.1 current_input and current_output on closed streams

`sics_current_input_1_04` calls `current_input(S)` on a closed input stream `S` and expects `domain_error(stream, S)`. The actual result is `existence_error(stream, '$stream'(3))`. `sics_current_output_1_04` is `current_output(S)` on a closed output stream, with the same discrepancy. The `domain_error(stream, Stream)` in 8.11.1.3 and 8.11.2.3 is for a term that is neither a variable nor a stream. The term of a closed stream is still a stream term, so we give `existence_error(stream, S)` for a stream that is not open (the same Formal as `set_input/1` in 8.11.3.3). `set_closed` and `out_closed` in `tests/pl/stream_prop.pl`, and closed streams in reading and writing, use the same Formal. The `stream_or_alias` for non-stream terms (the 8.11.1.3 decision above) is unchanged.

A or B?: A: `current_input/1` and `current_output/1` on a closed stream give `existence_error(stream, S)` (the current implementation). `sics_current_input_1_04` and `sics_current_output_1_04` remain failing. B: `domain_error(stream, S)`. A until confirmed. B is not implemented.

## Section 5 Autoload

Decision (2026-10-28): A. Library predicates such as `append/3`, `between/3`, and `format/2` are visible only after `use_module`. Nothing is imported into `user` at startup, and undefined predicates are not loaded on demand. This keeps the resolution rules of SPEC section 5 (own module, default import source, `system`), lets user programs define their own `append/3` and `member/2` without an import conflict, and matches the phase 6 rule that `mosac` links only the transitive closure of `use_module`. Conformance tests that call these predicates without a declaration are handled by the suite runner importing the six libraries into the test module, not by changing the system.

## Section 5 existence_error for procedures in modules

SPEC section 5 does not say how the culprit of `existence_error(procedure, PI)` names a predicate that is unknown in a module other than `user`. The ISO examples run in `user` and expect a bare `Name/Arity`.

A or B?: A: `Name/Arity` when the context module is `user`, and `Module:Name/Arity` otherwise. B: always `Name/Arity`. Not implemented until confirmed.

Decision (2026-10-28): A. Inside `user` the culprit stays `Name/Arity`, as the ISO examples expect; in any other module it is `Module:Name/Arity`, so the error shows where the lookup failed.

## Section 5 Import conflict

SPEC section 5 says a conflict between a local predicate and an imported one raises `permission_error(import_into, ...)` at load time, but does not fix the other two arguments.

A or B?: A: `permission_error(import_into, procedure, Name/Arity)`. B: `permission_error(import_into, procedure, Module:Name/Arity)`, where `Module` is the module the predicate comes from. Not implemented until confirmed.

Decision (2026-10-28): B. The culprit is `Module:Name/Arity` with the module the import comes from, so the error shows which import collided. This matches the module-qualified culprit chosen for `existence_error` above.

## Section 5 Module:Goal with a non-atom module

SPEC section 5 fixes `existence_error(module, M)` for an unknown module and leaves the case where `M` is neither a variable nor an atom (`1:true`). A variable gives `instantiation_error`.

A or B?: A: `type_error(atom, M)`. B: `type_error(module, M)`. Not implemented until confirmed.

Decision (2026-10-28): A. `1:true` gives `type_error(atom, 1)`. ISO 13211-1 has no `module` type, so the error stays within the existing type names.

## Section 5 Operators meta_predicate and as

The SPEC section 5 example writes `:- meta_predicate 全員(1, ?).` and `[姓名分解/3 as 分解]`, which can be read only if `meta_predicate` and `as` are operators. Neither is in the ISO operator table.

A or B?: A: add `op(1150, fx, meta_predicate)` and `op(700, xfx, as)` to the `system` operator table, so every module sees them. B: add neither; only `meta_predicate(...)` and `as(PI, Alias)` in canonical form are accepted. Not implemented until confirmed.

## 8.4.3 sort/2

`sort/2` is an ISO built-in (8.4.3), but the stage table in SPEC section 7 does not list it, and it is not implemented. The `sort_2` tests of the bundled suite are in the separate section of `docs/conformance.md`. `keysort/2` (8.4.4) is listed in stage G as system.

A or B?: A: implement `sort/2` in `system` in phase 4 Task 12, next to `keysort/2`, and add it to stage G of SPEC section 7. B: leave it to phase 5. Not implemented until confirmed.

## Section 7 forall/2

`forall/2` is listed in stage G of SPEC section 7 ("Non-ISO utilities (`library(*)`, the table in section 5)"), and section 5 names it among the predicates that must work across modules, but none of the six modules in the section 5 table exports it.

A or B?: A: implement `forall/2` in `system` as a meta predicate in phase 4 Task 9. B: do not implement it in phase 4. Not implemented until confirmed.
