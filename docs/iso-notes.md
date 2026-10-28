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

`lgt_write_term_3_118` to `122` expect the later of two repeated options to win: `[quoted(true), quoted(false)]` writes `A`, `[numbervars(true), numbervars(false)]` writes `$VAR(0)`, and the second `variable_names/1` is used. `lgt_open_4_41` (now blocked on `os::`) expects the same for the `open/4` option `eof_action/1`. The decision above gives `domain_error(write_option, LaterTerm)` (`opt_conflict` in `tests/pl/write.pl`). As far as we have found, the text of the standard (7.10.4 for write options, 7.10.2.11 for stream options) does not say what repeated options do.

A or B?: A: keep `domain_error(write_option, LaterTerm)` (the current decision; the 5 rows stay failing). B: the later option wins, as the suite assumes for `write_term/3` and `open/4`; `opt_conflict` changes in the same commit as the decision.

Decision (2026-10-28): B. The later of two repeated write options wins; every occurrence is still checked, so `[quoted(true), quoted(foo)]` gives `domain_error(write_option, quoted(foo))`. A second `variable_names/1` replaces the first. This replaces the 2026-10-25 rules "`domain_error(write_option, LaterTerm)`" and "`variable_names` is taken once". 8.14.2.3 gives `domain_error(write_option, E)` only when an element is not a valid write option, and each repeated element is valid. `open/4` already lets the later `type/1`, `reposition/1`, and `eof_action/1` win. Tests `opt_repeat_*` in `tests/pl/write.pl`.

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

`syntax/directives/discontiguous_1` (5 rows) fails to load because it writes `:- discontiguous((b/1, c/2)).`, a comma-separated sequence, which the decision above rejects ("述語指示子ではありません"). The single and list forms of the same file load, and `dc_arity2` to `dc_arity4` in `tests/pl/directives.pl` check that shape. As we read 7.4.2, the argument of `dynamic/1`, `multifile/1`, and `discontiguous/1` may be a predicate indicator, a predicate indicator sequence, or a predicate indicator list; we have not checked the clause text itself. `dynamic/1` and `multifile/1` reject the sequence in the same way.

A or B?: A: keep the decision (indicator or list only; the 5 rows stay failing). B: also accept a comma-separated sequence in all three directives, with the same checks on each element. A until confirmed.

Decision (2026-10-28): B. `dynamic/1`, `multifile/1`, and `discontiguous/1` also accept a comma-separated sequence `(p/1, q/2)`. Each element must be a predicate indicator and gets the same checks; a list or a bad element inside a sequence fails the load with the same diagnostic. This replaces "A comma-separated sequence is not an indicator" in the 2026-10-26 decisions for `discontiguous/1` and `multifile/1`. Tests `dc_seq_*`, `dyn_seq_*`, `mf_seq` in `tests/pl/directives.pl` and "comma sequence" in `tests/cpp/machine_test.cpp`.

## 7.4.2 multifile/1

Decision (2026-10-26): Directive only. The argument is a predicate indicator or a list of them, as with `dynamic/1`. A comma-separated sequence is not an indicator. `Module:PI` is not written until Phase 4. The declaration does not create a procedure. If there are no clauses, `current_predicate/1` fails and a call gives `existence_error`. The declaration carries over to the next load. For a predicate that carried over, the next load adds clauses and does not replace them. Loading the same text twice doubles the clauses. A predicate without the declaration is replaced, and its solutions are not doubled. A declaration that comes after the first clause in this load does not undo the replacement made by this load. That declaration remains, and the load after that adds clauses. Clauses are not added to a different arity or a different predicate. A failed load undoes the clauses and declarations it added. A predicate that is still dynamic stays dynamic after clauses are added. Gaps are not allowed, and the diagnostic remains `述語 名前/アリティ の節が離れている`.

Decision (2026-10-28): the comma-separated sequence is accepted; see 7.4.2 discontiguous/1.

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

Decision (2026-10-28): B. A `multifile/1` or `discontiguous/1` directive alone makes a procedure with no clauses, so calling it fails even with `unknown=error`. Only the call changes: `current_predicate/1` still does not report it, and `assertz/1` still creates it as dynamic, so a multifile hook can be asserted into. A load that fails drops the declaration with the rest (`src/machine/load.cpp` `declared_only`, `tests/pl/directives.pl` `*_only_call*`, `mf_hook_assert`, `tests/cpp/machine_test.cpp`).

## 8.11.1.3 Domain of current_input

Decision (2026-10-26): A. Passing a term that is neither a stream nor an alias to `current_input/1`, `current_output/1`, or `stream_property/2` gives `domain_error(stream_or_alias, S)`. Tests `input_bad`, `output_bad`, `prop_bad_stream`. The alias `user_input` succeeds with `current_input(user_input)`. Because aliases are in the domain, the Formal is `stream_or_alias`, not `stream`. This is recorded as a deviation from `domain_error(stream, S)` in 8.11.1.3 a and 8.11.8.3 a. Predicates that already use `stream_or_alias`, such as `set_input/1`, are unchanged.

## 8 with_output_to memory

Decision (2026-10-26): A. The memory stream created for `atom/1` and `codes/1` is closed on success, failure, exception, and halt. On success, the bytes are bound before the stream is closed. A closed slot is not reused. Afterwards, `stream_property` does not pick it up when enumerating. Passing that term gives `existence_error(stream, S)`. A stream passed in by the caller is not closed. The stream stays open while the goal runs, so `current_output` and `stream_property` inside the goal can see it.

## Conformance suite

Decision (2026-10-26): We do not wait for permission. `inriasuite.pl` has no redistribution permission, so it is not bundled. The Phase 3 conformance suite is Logtalk's `tests/prolog`, placed at commit `4decb64b598feff542a926a085eef01ec51f5a72` as is under `tests/conformance/logtalk/prolog`. Its license is Apache-2.0, the same as the repository, and the copyright notices are kept. The files are not modified. The 90% in Spec Section 9 is measured with this suite. This decision does not state a pass rate.

Decision (2026-10-28): A. The side-column directories (`between_3`, `succ_2`, `plus_3`, `format_2`, `format_3`, `atomic_list_concat_2`, `atomic_list_concat_3`) stay out of the run through phase 5, although the runner now imports the six libraries. The denominator stays 2171, so the phase 5 exit compares against the same baseline (2041). Library tests in `tests/pl/` cover these predicates.

## License

Decision (2026-10-26): The repository is unified under Apache-2.0. The text is in `LICENSE`. Mosa's copyright notice is `Copyright 2026 Mosa Prolog contributors`, placed in `NOTICE`. The Logtalk tests are under the same license, and their copyright stays as Paulo Moura's notice.

## 9.3.1.3 d (**)/2: negative power of 0

`lgt_power_2_14` expects `**(0, -1)` to give `evaluation_error(undefined)` or `evaluation_error(zero_divisor)`. ECLiPSe's IsoErrata says that the text of 9.3.1.3.d is undefined, and that it should be zero_divisor because infinity is defined. 9.3.10.3 d of Corrigendum 2 leaves the same form of `(^)/2` as undefined. Currently `std::pow(0, negative)` is infinity, so the result is `evaluation_error(float_overflow)`.

A or B?: A: `evaluation_error(undefined)`, following the text of 9.3.1.3.d. B: `evaluation_error(zero_divisor)`, following the errata. `float_overflow` is not changed before confirmation.

Decision (2026-10-28): A. Zero (integer, `0.0` or `-0.0`) to any negative power gives `evaluation_error(undefined)` for `(**)/2` and for the float form of `(^)/2`, as the integer form already did. When the `float_undefined` flag is not `error`, the result is NaN like the other undefined cases (`src/builtins/arith.cpp` `eval_float_bin`, `tests/pl/arith.pl` `pow_zero_*`).

## 7.5.4 Open scans and retract

`logical_update_semantics_retract` expects that even if `retract` runs during a scan, the clauses present at call time are all tried to the end. The same applies to the same pattern with `retractall` and `abolish`. ADR 0004 decided that retract removes the clause from subsequent candidates and keeps the rest of the current clause. `retract_open_later` in `tests/pl/database.pl` is `[X = 1, X = 2]`; the removed `later(3)` does not appear.

A or B?: A keeps ADR 0004: removed clauses are dropped from open scans. B follows 7.5.4: the call tries the initial sequence. Stay with A until confirmed.

Decision (2026-10-28): B. The design is in ADR 0008, which replaces the retract and abolish part of ADR 0004. ADR 0008 was approved and implemented on 2026-10-28 (dead clauses in `src/builtins/database.cpp`; `tests/pl/database.pl` `lu_*`, `retract_open_later`, and `tests/pl/solutions.pl` `scan_retract` now expect every clause present at call time).

## 8.11.8 position with reposition(false)

`sics_set_stream_position_2_06` takes `stream_property(Stream, position(Position))` from a stream opened with `reposition(false)` and expects `set_stream_position(Stream, Position)` to give `permission_error(reposition, stream, Stream)`. With the file copied in, the actual result is `fail`. `stream_property/2` does not report `position/1` for a stream that cannot be repositioned ("no position" in `tests/cpp/stream_test.cpp`). `set_stream_position/2` already throws `permission_error(reposition, stream, Stream)` when given a position term for such a stream ("reposition formal" in the same file).

A or B?: A: do not report the property (the current implementation; the goal fails). B: report `position/1`, then give `permission_error`. A until confirmed. B is not implemented.

Decision (2026-10-28): A. A stream that cannot be repositioned does not report `position/1`; the position would have no use. `sics_set_stream_position_2_06` stays failing as a difference in interpretation, and "no position" in `tests/cpp/stream_test.cpp` is unchanged.

## memberchk/2 and iso_stream_property_2_01

`memberchk/2` belongs to `library(lists)` and is not made a built-in in Phase 3. The assertion of `iso_stream_property_2_01` runs the helper clauses in the test file as the user clauses present at that time. The reading that leaves the row as `fail` is withdrawn.

A or B?: A: do not add a built-in `memberchk/2`. B: add it as a built-in. A until confirmed. B is not implemented.

Closed (2026-10-28): since phase 5 Task 2 the suite runner imports `library(lists)` into the test machine, so `iso_stream_property_2_01` passes without a built-in `memberchk/2`. The question no longer decides any row and is withdrawn; nothing is added.

## 8.11.1 current_input and current_output on closed streams

`sics_current_input_1_04` calls `current_input(S)` on a closed input stream `S` and expects `domain_error(stream, S)`. The actual result is `existence_error(stream, '$stream'(3))`. `sics_current_output_1_04` is `current_output(S)` on a closed output stream, with the same discrepancy. The `domain_error(stream, Stream)` in 8.11.1.3 and 8.11.2.3 is for a term that is neither a variable nor a stream. The term of a closed stream is still a stream term, so we give `existence_error(stream, S)` for a stream that is not open (the same Formal as `set_input/1` in 8.11.3.3). `set_closed` and `out_closed` in `tests/pl/stream_prop.pl`, and closed streams in reading and writing, use the same Formal. The `stream_or_alias` for non-stream terms (the 8.11.1.3 decision above) is unchanged.

A or B?: A: `current_input/1` and `current_output/1` on a closed stream give `existence_error(stream, S)` (the current implementation). `sics_current_input_1_04` and `sics_current_output_1_04` remain failing. B: `domain_error(stream, S)`. A until confirmed. B is not implemented.

Decision (2026-10-28): B. `current_input/1` and `current_output/1` give `domain_error(stream, S)` for the term of a closed stream, the only error 8.11.1.3 and 8.11.2.3 list. A term that was never a stream, such as `'$stream'(99)`, still gives `existence_error(stream, S)`, and a term that is neither a variable nor a stream still gives `domain_error(stream_or_alias, S)` (the 8.11.1.3 decision above). Other built-ins keep `existence_error(stream, S)` for a closed stream (`src/builtins/stream_prop.cpp` `match_current`, `tests/pl/stream_prop.pl` `input_closed`, `output_closed`).

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

Decision (2026-10-28): A. Both operators are added to the `system` operator table, so the SPEC section 5 example reads as written in every module. This is a deviation from the ISO operator table (6.3.4.4): a program that uses `as` as a plain atom in an operator position reads differently.

## Section 5 Operator :

SPEC section 5 writes `?- 家系:祖父(太郎, Z).`, which can be read only if `:` is an infix operator. `:` is not in the ISO operator table (6.3.4.4), and `src/read/operator_table.cpp` leaves it out on purpose.

A or B?: A: add `op(600, xfy, :)` to the `system` operator table, the same as SWI-Prolog and as the operator the Logtalk judge adds for a moment. B: add nothing; only `:(M, G)` in canonical form is accepted.

Decision (2026-10-28): A. `op(600, xfy, :)` is in the `system` operator table. This is a deviation from the ISO operator table (6.3.4.4), like `meta_predicate` and `as` above.

## Section 5 Cut inside Module:Goal

SPEC section 5 says `Module:Goal` runs `Goal` in the context of `Module`, but not how far a cut inside `Goal` reaches. ISO 13211-1 7.8 makes `call/1` opaque to cut and `,/2`, `;/2`, `->/2` transparent.

A or B?: A: transparent; the `!` in `p :- member(X, L), user:!.` cuts the clause of `p`, as SWI-Prolog compiles a qualified body goal. B: opaque, like `call/1`; the cut is local to `Module:Goal`.

Decision (2026-10-28): A. `:/2` is transparent to cut. A goal reached through `call/N` stays opaque, because `call/N` itself sets the cut barrier.

## Section 5 Module:Goal with a non-callable goal

SPEC section 5 does not fix the culprit of `type_error(callable, ...)` when `Goal` in `Module:Goal` cannot be called, as in `user:(fail, 1)`. ISO 13211-1 7.8.3.3 gives the argument of `call/1` as the culprit.

A or B?: A: the whole `Module:Goal`, so `call(user:(fail, 1))` reports the term that was passed to `call/1`, and `call(user:1, a)` already reports `user:1`. B: only `Goal`, as in `(fail, 1)`.

Decision (2026-10-28): A. The culprit is the whole `Module:Goal`.

## Section 5 Conflict between two imports

SPEC section 5 says local and imported predicates share one namespace, and fixes the error for a local definition that meets an import. It does not say what happens when one module imports the same `Name/Arity` from two different modules.

A or B?: A: the second import fails the load with `permission_error(import_into, procedure, From:Name/Arity)`, where `From` is the module of the second import; importing again from the same module is allowed. B: the later import silently replaces the earlier one.

Decision (2026-10-28): A. Two sources for one name are a conflict, with the same Formal as a local definition that meets an import.

## Section 5 Importing a predicate that is not exported

SPEC section 5 lets `use_module(File, Imports)` name the predicates to import, but does not say what happens when a named predicate is not in the export list of the loaded module.

A or B?: A: the load fails with `existence_error(procedure, Module:Name/Arity)`. B: import it anyway, as SWI-Prolog does with a warning. In both, an entry of `except([...])` that is not exported is ignored.

Decision (2026-10-28): A. Only exported predicates can be imported; a missing one fails the load with `existence_error(procedure, Module:Name/Arity)`.

## Section 5 use_module of a file that is not a module

SPEC section 5 says one file corresponds to one module, but not what `use_module/1,2` does with a file whose first term is not `module/2`.

A or B?: A: the load fails with `permission_error(load, source_sink, File)` and the clauses it read are rolled back, as SWI-Prolog rejects a non-module file. B: read it like `consult/1` into the context module, with nothing to import.

Decision (2026-10-28): A. `use_module/1,2` accepts module files only.

## Section 5 A file left half-read by an initialization exception

When a nested load's `initialization/1` goal throws, the clauses read so far are kept and the outer file stops at that directive. The outer file stayed marked as loaded, so a later `use_module/1,2` or `ensure_loaded/1` did not read the rest and saw a half-built module.

A or B?: A: remove the mark of every file that was not read to the end, so the next `use_module/1,2` or `ensure_loaded/1` reads it again; clauses and modules stay as now. B: keep the mark.

Decision (2026-10-28): A.

## Section 5 Clause operations on an unknown module

SPEC section 5 allows `assertz(家系:親(a, b))` but does not say what happens when the module does not exist yet.

A or B?: A: `existence_error(module, M)`, the same as `M:G`. B: create the module and add the clause, as SWI-Prolog does.

Decision (2026-10-28): A. Only `module/2` creates a module. `asserta/1`, `assertz/1`, `retract/1`, `retractall/1`, `clause/2` and `abolish/1` with an unknown `M:` give `existence_error(module, M)`.

## Section 5 Clause operations on an imported name

SPEC section 5 says a predicate created by `assertz/1` belongs to the module that added it, but not what happens when the context module imports that name.

A or B?: A: follow the import, as a call does, and act on the definition in the source module. B: look only at the context module's own predicates; `assertz/1` then conflicts with the import.

Decision (2026-10-28): A. The database built-ins look at the module's own predicates, then its explicit imports (`use_module/1,2` and the exports of a consulted module file). They do not follow the default import source, so `assertz(p(1))` inside a module never adds to `user:p/1`. As with the existence error, `permission_error(modify, static_procedure, PI)` names a predicate outside `user` as `M:N/A`.

## 8.8.2 current_predicate/1 in modules

SPEC section 5 does not say which predicates `current_predicate(N/A)` and `current_predicate(M:N/A)` enumerate.

A or B?: A: the module's own predicates only; with a variable `M`, every module is enumerated and `M` is bound. B: own and imported predicates.

Decision (2026-10-28): A. An unqualified `current_predicate/1` enumerates the context module's own predicates, so the result in `user` is the same as before modules.

## 8.4.3 sort/2

`sort/2` is an ISO built-in (8.4.3), but the stage table in SPEC section 7 does not list it, and it is not implemented. The `sort_2` tests of the bundled suite are in the separate section of `docs/conformance.md`. `keysort/2` (8.4.4) is listed in stage G as system.

A or B?: A: implement `sort/2` in `system` in phase 4 Task 12, next to `keysort/2`, and add it to stage G of SPEC section 7. B: leave it to phase 5. Not implemented until confirmed.

Decision (2026-10-28): A. `sort/2` is implemented in `system` in phase 4 Task 12 next to `keysort/2`, with the 8.4.3.4 Examples as tests. Stage G of SPEC section 7 now lists it.

## Section 7 forall/2

`forall/2` is listed in stage G of SPEC section 7 ("Non-ISO utilities (`library(*)`, the table in section 5)"), and section 5 names it among the predicates that must work across modules, but none of the six modules in the section 5 table exports it.

A or B?: A: implement `forall/2` in `system` as a meta predicate in phase 4 Task 9. B: do not implement it in phase 4. Not implemented until confirmed.

Decision (2026-10-28): A. `forall/2` is implemented in `system` as a meta predicate in phase 4 Task 9, as `\+ (Cond, \+ Action)` run in the caller's context module. It is the only stage G predicate outside the six modules that phase 4 adds.

## ISO/IEC 13211-2 7.2.2 predicate_property/2

SPEC section 5 lists `predicate_property/2` in stage F but does not say what a variable head or an unknown module gives.

A or B?: A: a variable head gives `instantiation_error`, as in the 13211-2 DTR and the bundled suite (`commons_predicate_property_2_16`); an unknown `M:` gives `existence_error(module, M)`, the same as `M:G`. B: enumerate the visible predicates, as SWI-Prolog does, and fail for an unknown module.

Decision (2026-10-28): A for both. A built-in has `defined`, `built_in` and `static`, and a control construct or goal-taking built-in also has `meta_predicate(Head)` (for example `findall(?, 0, -)`). A user predicate is looked up as a call would be and has, in this order, `defined`, `dynamic` or `static`, `exported`, `imported_from(M)` (only through an explicit import of the context module), `multifile`, `meta_predicate(Head)` (when declared) and `number_of_clauses(N)`. Any other property term gives `domain_error(predicate_property, P)`.

## Section 5 Operators in modules

SPEC section 5 makes `op/3` local to the declaring module, lets `op(P, T, N)` in an export list reach importers, and makes `user` operators visible everywhere. It does not say what `op(0, T, N)` does inside a module, or whether an import list `[PI, ...]` brings exported operators.

A or B?: A: a local `op(0, T, N)` hides the `user` and ISO core operator of that class inside the module only, as in SWI-Prolog; exported operators come in with `use_module/1`, `except(...)` and a consulted module file, but not with an import list. B: a local `op(0, T, N)` removes only the module's own entry; exported operators come in with every form of import.

Decision (2026-10-28): A for both. Lookup is the module's own layer, then the `user` table (which holds the ISO core). `op/3`, `current_op/3`, `read_term/2,3` and the `write` family use the caller's context module. A `:- op` directive in a module file defines into that module. Importing into `user` puts the operator in the `user` table, so every module sees it.

## 7.4.2.8 Per-file flags and initialization/1

SPEC section 5 restores `double_quotes` and `japanese_punctuation` per file being loaded. 7.4.2.8 runs an `initialization/1` goal after the Prolog text has been prepared, but does not say whether it sees the flags the file set.

A or B?: A: run the goals before restoring, so they see the file's flags. B: restore first, so they see the same flags as a query run right after the load.

Decision (2026-10-28): B. The file's own terms are already read with its flags; only reads made by the goal at run time and `current_prolog_flag/2` see the difference. A goal that wants the file's syntax sets the flag itself.

## Section 5 succ/2 with a negative integer

`succ/2` is not in the standard. The phase 4 plan expected `type_error(not_less_than_zero, -1)` (as in SWI-Prolog), while the Logtalk suite (`lgt_succ_2_09`, `lgt_succ_2_10`) expects `domain_error(not_less_than_zero, -1)`.

A or B?: A: `domain_error(not_less_than_zero, N)`, the same as `length/2`. B: `type_error(not_less_than_zero, N)`.

Decision (2026-10-28): A. `not_less_than_zero` is a domain in 7.12.2 c, and `length/2` already raises it as a domain error.

## Section 5 format/2 errors

`format/1,2,3` is not in the standard. The Logtalk suite (`format_2`) fixes the argument errors in ISO form: count and numeric arguments of `~c`, `~d`, `~e`, `~f`, `~n` and `~*` are evaluated as by `is/2`, `~d` needs an integer result (`type_error(integer, X)`), and `~a` needs an atom (`type_error(atom, X)`). For too few or too many arguments it accepts any `domain_error(_, _)`.

A or B?: A: a Formal per problem: `domain_error(non_empty_list, [])` when the arguments run out, `domain_error(empty_list, Rest)` when some are left, and `domain_error(format_directive, C)` for an unknown directive character `C` (`'~'` for a lone `~` at the end). B: `domain_error(format_control, Format)` for all three, with the whole format as the culprit.

Decision (2026-10-28): A. `non_empty_list` is a domain in 7.12.2 c, and the culprit tells which part is wrong. A negative count is `domain_error(not_less_than_zero, N)`, as for `length/2`.

## 7.9.1 Which error first: the functor or its arguments

In `X is foo(bar)`, both `foo/1` and `bar/0` are not evaluable. 7.12.1 leaves the choice between two error conditions to the implementation. Mosa evaluated the arguments first and reported `bar/0`; the Logtalk suite (`format_2`, `~d` and `~f` with `foo(bar)`) expects `foo/1`.

A or B?: A: keep evaluating the arguments first (`type_error(evaluable, bar/0)`). B: check the principal functor before evaluating its arguments (`type_error(evaluable, foo/1)`).

Decision (2026-10-28): B. 7.9.1 dispatches on whether the principal functor is evaluable before it evaluates the arguments, and a bad outer functor stops evaluation before any work on the arguments.

## Section 5 atomic_list_concat/2,3 with a number as the result

`atomic_list_concat/2,3` is not in the standard. The Logtalk suite (`atomic_list_concat_2`, `atomic_list_concat_3`) expects `type_error(atom, a(1))` when the result argument is a compound, but does not say what a number such as `12` in `atomic_list_concat([1, 2], 12)` should do.

A or B?: A: `type_error(atom, 12)`; the result argument must be a variable or an atom. B: compare the text, so the goal succeeds.

Decision (2026-10-28): A. 8.16.2.3 gives `atom_concat/3` the same rule for its third argument, and it matches the Logtalk compound case.

## Section 5 atomic_list_concat/3 splitting on an empty separator

When `atomic_list_concat/3` splits (the list is open and the atom is bound), an empty separator `''` does not give one way to split.

A or B?: A: `domain_error(non_empty_atom, '')`. B: fail, or split into single characters.

Decision (2026-10-28): A. An error says what is wrong instead of failing quietly, and splitting into characters is the job of `atom_chars/2`.

## Section 5 Clause operations through an `as` alias

After `use_module(別源, [静/1 as 静か])`, `assertz(静か(2))` tries to change the static procedure `別源:静/1` through the alias `静か/1`. 8.9.1.3 names the procedure in `permission_error(modify, static_procedure, PI)` but does not say which name to use when the caller reached it through an alias.

A or B?: A: the source procedure, `別源:静/1`, the same form as for a plain import (`家系:親/2`). B: the alias the caller wrote, `静か/1`.

Decision (2026-10-28): A. The error is about the procedure that cannot be changed, and the alias is only another name for it. A plain import already reports the source, and the module in the culprit says which declaration to make dynamic.

## Section 3 if/3

`if/3` is not in the standard or in SPEC Section 3. The 12 tests in `control/if_3` call it and get `existence_error(procedure, if/3)`.

A or B?: A: outside SPEC, so it is not implemented, and the 12 rows stay failing. B: add `if/3` to SPEC Section 3 and implement it. A until confirmed.

## Section 3 Flags dialect and version_data

The flags `dialect` and `version_data` are not in 7.11 or in SPEC Section 3. `commons_current_prolog_flag_2_16` to `18` get `domain_error(prolog_flag, dialect)` and `domain_error(prolog_flag, version_data)`.

A or B?: A: outside SPEC, so they are not implemented, and the 3 rows stay failing. B: add `dialect` (`mosa`) and `version_data` (`mosa(Major, Minor, Patch, [])`) to SPEC Section 3 and implement them. A until confirmed.

## Section 4 Stream encoding and BOM

The 21 failing rows in `unicode/builtins` expect the IANA name `'UTF-8'`, a BOM, and no encoding on binary streams. SPEC Section 4 and Section 3 item 7 already fix the flag `encoding` to `utf8` and give every stream an `encoding` option that defaults to `utf8`, and Mosa follows that: `current_prolog_flag(encoding, utf8)`, `open/4` accepts `encoding(utf8)`, and binary streams also report `encoding(utf8)`. So `open/4` with `encoding('UTF-8')` gives `domain_error(stream_option, encoding('UTF-8'))`, `lgt_unicode_current_prolog_flag_2_02` fails on the name, and `lgt_unicode_stream_property_2_07` and `09` fail because a binary stream reports `encoding/1`. The `open/4` option `bom/1`, the stream property `bom/1`, and skipping a BOM when a text stream is read are in neither the standard (7.10.2) nor SPEC.

A or B?: A: keep SPEC as it is (the name `utf8` only, every stream reports `encoding/1`, no BOM handling), and the 21 rows stay failing. B: amend SPEC Section 4 to accept `'UTF-8'` as another name, drop `encoding/1` from binary streams, add the `bom/1` option and property, and skip a BOM on text input, then implement them. A until confirmed.

Decision (2026-10-28): A. These options, properties, and names are outside the standard, and SPEC already fixes the encoding to `utf8` on every stream. The 21 rows stay failing as a difference in interpretation. Skipping a BOM on text input may be asked separately in Task 15.

## 9.3.10.3 (^)/2 with a negative integer exponent

`iso_integer_power_2_06` expects `2^(-1)` to give `type_error(float, 2)`. `src/builtins/arith.cpp` reads 9.3.10.3 e of Corrigendum 2 as giving the float type error only when the exponent is less than -1, and gives `evaluation_error(undefined)` for `2^(-1)`.

A or B?: A: keep the current reading (`evaluation_error(undefined)`). B: any integer base other than 1 and -1 with a negative integer exponent gives `type_error(float, Base)`. A until confirmed.

## 8.11 Properties of the standard streams

`sics_stream_property_2_05c` and `lgt_stream_property_2_08b` expect `user_input` to report `eof_action(reset)`. `sics_stream_property_2_06d`, `lgt_stream_property_2_09c`, and `lgt_stream_property_2_10c` expect `user_output` and `user_error` to report `mode(append)`. Mosa reports `eof_action(eof_code)` (the default in "8.11 Reading past end of stream") and `mode(write)`.

A or B?: A: keep `eof_code` and `write`. B: `user_input` has `eof_action(reset)`, and `user_output` and `user_error` have `mode(append)`. A until confirmed.

## Spec Section 4: So characters are symbol characters

`lgt_unicode_escape_sequence_full_05` expects `writeq('🀙🀚🀛🀜🀝🀞🀟🀠')` to quote the atom. SPEC Section 4 makes non-ASCII characters in `So` symbol characters, so these 8 characters form a symbol-character atom (6.4.2), and `writeq` (7.10.5) writes it without quotes. The row stays failing as a difference in interpretation. This follows the approved SPEC, so no question is open.

## 7.10.5 a Writing a variable that occurs once

`lgt_write_term_3_043`, `044`, `065`, and `066` expect a written variable to be `_` followed by at least one more character. Mosa writes a variable that occurs once in the term as `_` alone, and a variable that occurs more than once as `_G` followed by a number (`f(_G1,_G1,_)`). 7.10.5 a says only that the text of a variable is implementation dependent and begins with `_`.

A or B?: A: keep `_` for a variable that occurs once (the current writer). B: give every variable a name with at least one character after `_`, such as `_G1`. A until confirmed.

## 9.1.6 Rounding functions given an integer

`lgt_ceiling_1_05`, `lgt_floor_1_06`, `lgt_round_1_13`, `lgt_truncate_1_06`, `lgt_float_integer_part_1_04`, and `lgt_float_fractional_part_1_04` expect `X is floor(9)` and the like to give `type_error(float, 9)`. 9.1.6 gives `floor`, `ceiling`, `round`, and `truncate` the signature F → I, and `float_integer_part` and `float_fractional_part` the signature F → F, so an integer argument is a type error. Mosa returns the integer for the first four (`floor(9) = 9`) and a float for the last two (`float_integer_part(9) = 9.0`). SPEC lists these evaluables but says nothing about their argument types.

A or B?: A: follow 9.1.6 and give `type_error(float, N)` for an integer argument to all six. B: keep accepting integers (the current behaviour). B until confirmed.

Decision (2026-10-28): A. An integer argument to these six gives `type_error(float, N)`, where `N` is the evaluated value (`floor(4 + 5)` gives `type_error(float, 9)`). `integer/1` is not one of them and still accepts integers (`src/builtins/arith.cpp` `float_only`, `tests/pl/arith.pl`). Mosa has no `iso` flag yet, so this holds in every mode.

## 9.1.6 round/1 of a negative half

`eclipse_round_1_07` and `08` expect `round(-3.5)` to be `-3` and `round(-4.5)` to be `-4`. 9.1.6 defines `round(x)` as `floor(x + 1/2)`, which rounds a half towards positive infinity. Mosa uses `std::round`, which rounds a half away from zero, so it gives `-4` and `-5`. Positive halves give the same result either way (`round(2.5) = 3`).

A or B?: A: keep rounding a half away from zero (`round(-2.5) = -3`). B: follow 9.1.6, `floor(x + 1/2)` (`round(-2.5) = -2`). A until confirmed.

Decision (2026-10-28): B. `round/1` follows 9.1.6 and rounds a half towards positive infinity. The fractional part is taken by subtracting `floor(x)`, which is exact, so `0.49999999999999994` and odd integers above 2^52 are not rounded twice (`src/builtins/arith_float.cpp`, `tests/pl/arith.pl` `round_*`).
