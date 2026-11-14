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

Decision (2026-10-29): owner chose A. The number parsing shared with `number_chars/2` and `number_codes/2` gives `syntax_error(Key)` with the ADR 0007 keys instead of the text: a lexical failure gives its reason key (`integer_overflow`, `bad_escape`, `unterminated_comment`, …), anything else that is not exactly one number gives `unexpected_token`.

Since phase 10 (2026-11-12, ADR 0025), an integer token of any length is a number, so the key `integer_overflow` in the list above no longer exists; the other keys are unchanged.

Decision (2026-10-29): owner chose A over the SWI behaviour. A bound first argument that is not an atom gives `type_error(atom, A)`, and a bound second argument that is not a number gives `type_error(number, N)`. Both are checked before the atom is parsed, as `number_chars/2` does.

## 8.10 findall/4 (non-ISO)

Decision (2026-10-25): `findall(Template, Goal, List, Tail)` is a difference list. If there are no solutions, List is unified with Tail; Tail is not bound to `[]`. The fourth argument is not checked as a list. `findall/3` is `findall/4` with the tail `[]`.

Decision (2026-10-27): The "not checked" above is withdrawn. If the fourth argument is neither a partial list nor a list, throw `type_error(list, Tail)` before collecting. A variable tail is bound at the end of the solutions. An empty list and a proper list also remain the tail. The tail is not bound before the check finishes.

## 8.14 print/1 and print/2

Decision (2026-10-25): Open question 4: B. `print/1` and `print/2` do not call `portray/1` and output the same text as `write/1`. Registered under term output.

Decision (2026-10-27): The B above is withdrawn. When `portray/1` has clauses and the goal succeeds, `print/1` and `print/2` use its output as the text of the term. When it has no clauses, the procedure is the same as `write/1`. When it has clauses but fails, that term gets the same text as `write/1`, and only the arguments of a compound term are passed through `print` again. `portray/1` is not made a built-in. `write/1` does not look at `portray`. Output written by a failed `portray` is not kept.

Review L14 (2026-11-05, not an owner decision): when `portray` halts, the text of the term written so far, with what `portray` wrote to current output, is written to the stream before the halt, as with `write/1` followed by `halt`. As before, `print` writes its text in one piece after the term is done, so what `portray` writes to another stream by name comes first. When `portray` raises an exception, nothing of that `print` is written, as before, because a `catch` may go on writing.

## 8.14 Write options

Decision (2026-10-25): `write/1,2` and `print/1,2` use `numbervars(true)`. `writeq/1,2` uses `quoted(true)` and `numbervars(true)`. `write_canonical/1,2` uses `quoted(true)` and `ignore_ops(true)`. In `write_term` all three default to false. When values of the same boolean option conflict, `domain_error(write_option, LaterTerm)`. `variable_names` is taken once; if a name is not a variable spelling, `domain_error(write_option, ThatPair)`. A Var that is already bound is ignored.

`lgt_write_term_3_118` to `122` expect the later of two repeated options to win: `[quoted(true), quoted(false)]` writes `A`, `[numbervars(true), numbervars(false)]` writes `$VAR(0)`, and the second `variable_names/1` is used. `lgt_open_4_41` (now blocked on `os::`) expects the same for the `open/4` option `eof_action/1`. The decision above gives `domain_error(write_option, LaterTerm)` (`opt_conflict` in `tests/pl/write.pl`). As far as we have found, the text of the standard (7.10.4 for write options, 7.10.2.11 for stream options) does not say what repeated options do.

A or B?: A: keep `domain_error(write_option, LaterTerm)` (the current decision; the 5 rows stay failing). B: the later option wins, as the suite assumes for `write_term/3` and `open/4`; `opt_conflict` changes in the same commit as the decision.

Decision (2026-10-28): B. The later of two repeated write options wins; every occurrence is still checked, so `[quoted(true), quoted(foo)]` gives `domain_error(write_option, quoted(foo))`. A second `variable_names/1` replaces the first. This replaces the 2026-10-25 rules "`domain_error(write_option, LaterTerm)`" and "`variable_names` is taken once". 8.14.2.3 gives `domain_error(write_option, E)` only when an element is not a valid write option, and each repeated element is valid. `open/4` already lets the later `type/1`, `reposition/1`, and `eof_action/1` win. Tests `opt_repeat_*` in `tests/pl/write.pl`.

## 8.14.3 op/3

Decision (2026-10-26): The goal calls `OperatorTable::define`. Checks run in the order instantiation, type, domain, permission; for a list of names, every name is checked before the table is replaced. On rejection the table is not changed. A nonzero declaration replaces the old specifier of the same class (prefix, infix, postfix). Priority 0 removes only that specifier. Priority 0 for `|` succeeds as a removal, as in the Cor.2 example. Infix with priority from 1 to 1000 inclusive, as well as prefix and postfix, give `permission_error(create, operator, '|')`. Comma gives `permission_error(modify, operator, ',')`. `[]` and `{}` give `permission_error(create, operator, ThatAtom)`. The `:- op` directive uses the same `define`.

Question (2026-11-02, review M4): since 45bbc64 an op/3 call made after a choice point or inside an armed catch/3 was undone when execution backtracked to that point or the catch received a throw, so `findall(x, op(700, xfx, qq2), _)` left no `qq2`. 8.14.3 makes the change a side effect of the goal and says nothing about undoing it, and assertz/1, set_prolog_flag/2 and char_conversion/2 were never undone. A or B?: A: op/3 is never undone by backtracking, exceptions, findall/3 or `\+`. B: keep undoing it and record it as an intentional deviation.

Decision (2026-11-02): A. A table change stays from the moment op/3 succeeds, in mosa and in mosac executables, for the `user` table and for a module's own layer. A load that fails still restores the table it started with (the loader's own rollback, not this rule). Tests `m4_*` and `op_backtrack_prefix` in `tests/pl/operators.pl`, `m4_local_backtrack` in `tests/pl/modules.pl`.

## 8.14.4 current_op/3

Decision (2026-10-26): Checks follow argument order, and the first one that applies is thrown. If the priority is not a variable and not an integer from 0 to 1200 inclusive, `domain_error(operator_priority, Priority)`. Atoms and floats also get this domain error. If the specifier is not a variable and not one of `xf` `yf` `xfx` `xfy` `yfx` `fx` `fy`, `domain_error(operator_specifier, Op_specifier)`. Integers and compound terms also get this domain error. If the operator is neither a variable nor an atom, `type_error(atom, Operator)`. Priority 0 is in the domain. If nothing matches, the goal fails. Candidates are copied at call time; even if `op/3` changes the table during backtracking, the copied candidates remain. When the name is an atom, the order is the same as `lookup`: ascending priority, and within the same priority, specifier declaration order. When the name is a variable, the order is table order.

## 8 with_output_to/2

Decision (2026-10-25): Only the first solution is taken. On success, `atom/1` and `codes/1` are bound. On failure and exception, nothing is bound. The current output is restored on success, failure, and exception alike. The goal runs in the current context. The memory stream it creates is closed when done (see "8 with_output_to memory" below).

## 8.8 clause/2

Decision (2026-10-25): Open question 5: A. User-defined predicates are public. This includes static clauses from `consult`, `:- dynamic`, predicates created by `assert`, and dynamic predicates whose clauses were all removed by `retract`. `clause/2` returns clauses in definition order. The body of a fact is the atom `true`. Variables in the same slot are the same cell; an anonymous variable is a fresh reference at each occurrence. `false/0` in a body is folded to `fail` by the compiler, so the returned body is the atom `fail`. Built-ins and control constructs give `permission_error(access, private_procedure, PI)`. A nonexistent predicate fails. A dynamic predicate with no clauses fails but is not private. Static user predicates can be read.

## 8.9.1 asserta/1 and assertz/1 with a variable body

8.9.1.3 and 8.9.2.3 raise `instantiation_error` only for a variable head; 7.6.2 converts a variable body `X` to `call(X)`. Mosa stored `p :- X` that way when it came from `consult` or `mosac`, and also `p(X) :- true, X` from assert, but `asserta((p :- X))` and `assertz((p(X) :- X))` raised `instantiation_error` (review H5).

A or B?: A follows 7.6.2: the clause behaves as if its body were `call(X)`. `clause(p, B)` gives `B = call(_)`, `retract((p :- B))` unifies `B` with `call(_)` (so `retract(p)` and `retract((p :- true))` do not remove it), and calling it with `X` unbound raises `instantiation_error` from `call/1`. B keeps the error and records it as an intentional deviation.

Decision (2026-11-01): A (the owner asked for H5 to be planned and implemented in one request). retract/1 used to unify the stored IR variable itself, which matched any body, also for `p(X) :- true, X` and consulted clauses before H5; it now builds the body as clause/2 does (this also makes `retract((h :- halt))` match, and `false` in a body is seen as `fail`, as this file records under 8.8 clause/2). The arguments of `catch/3` are left as written, as 7.6.2 c says; clause/2 used to show a variable there as `call(X)`. `retract((p(Y) :- Y))` on `p(X) :- X` unifies `Y` with `call(Y)`, which is not subject to occurs check (7.3.3), so `Y` becomes a cyclic term. Tests: `tests/pl/database.pl` `assert_body_var_*`.

## 7.8.9 catch/3 goals that contain variables

7.8.9.1 runs the Goal of `catch/3` as `call/1` does, so a variable inside it is replaced only when the catch runs, and a `!` it is bound to cuts the Goal of the catch. Mosa compiles the Goal when the clause is made and turns such a variable into `call(X)`, which is opaque to cut: after `assertz((t(X) :- catch(((X, fail) ; true), _, true)))`, `t(!)` succeeds (mosa and mosac), while `G = catch(((X, fail) ; true), _, true), X = !, call(G)` fails. Since H5, clause/2 shows the Goal as written (7.6.2 c), so `clause(t(!), B), call(B)` fails while `t(!)` succeeds. Found by the adversarial review of H5; the compile step predates H5.

A or B?: A compiles the Goal and Recovery of catch/3 so that a variable in them is transparent to cut when it is bound at run time (an IR change, so an ADR first). B keeps the compiled form and records it here as a deviation.

Decision (2026-11-01): A. Planned after H6: only a Goal or Recovery with a variable in goal position is kept as data and run by `call/1` at run time; catch/3 without such a variable stays compiled. ADR first (IR and mosac codegen change). Review item M14.

Done (2026-11-01, 0b333c3, ADR 0014): such a Goal or Recovery becomes an `IrTag::Call` node and runs as `call/1` when the catch runs; mosac no longer translates `\+` and `M:G` data with a variable in goal position ahead (n(!) used to differ from mosa). Tests: `tests/pl/control.pl` `m14_*`, `tests/mosac/undefined_catch.pl`, `tests/stack/deep_builtins.pl` `call_catch_late_nest`. Done (2026-11-01, review M15, ADR 0014 amendment): without a variable, a Goal or Recovery with a non-callable part (a number in goal position below `,` `;` `->` `*->`) also becomes a `Call` node, so `catch((write(a), 1), E, true)` binds `E` to `type_error(callable, (write(a), 1))` without writing `a`, and `call/1` and `assertz/1` no longer look into catch/3 arguments. Tests: `tests/pl/control.pl` `m15_*`.

## 7.3.3 Calling a cyclic goal

7.3.3 leaves terms that are not subject to occurs check (cyclic terms) undefined. Mosa used to crash on `X = f(X), call(X)` and `X = call(X), call(X)` (SIGSEGV) and never return on `X = (a, X), call(X)` (review M13). Since review M13 (2026-11-01) a goal built at run time runs as its unfolding: a predicate call whose arguments contain the goal itself is an ordinary call with that cyclic argument, and a control construct (`,` `;` `->` `*->` `catch/3`) that leads back to one being translated becomes a call/1 of the original term at that point. So `X = (a, X)` raises `existence_error(procedure, a/0)` and `X = (Y = 1 ; X)` gives `Y = 1` again on each retry. A cut is local to the pass it is in: `X = (((Y == x, !) ; (Y = x, X)) ; Y = extra), findall(Y, X, L)` gives `[x, extra]`, while the same goal unfolded once by hand gives `[x]`. A goal that never stops unfolded never stops here either: `X = call(X)`, `X = user:X`, `X = (true, X)` and `X = (fail ; X)` run forever within the heap limit (as `p :- (true, p)` does), left recursion such as `X = (X, true)`, `X = catch(X, _, true)`, `X = (X -> true ; true)`, `X = (\+ X)` and `X = (X ; true)` stops at the heap limit, which since review M17 also counts the choice points and other state outside the heap (ADR 0010 amendment 3; under `--heap-max 64` these stop at 95 to 122 MB RSS, where they used to reach 946 MB to 1.6 GB, and `X = (X ; true)` grew without end). `with_output_to/2` still recurses natively (7.1). Tests: `tests/pl/control.pl` `m13_*`, `tests/cpp/machine_test.cpp` `check_cyclic_goal_limit`, `tests/stack/deep_builtins.pl` `call_cyclic_leaves`.

A clause has no environment to point back into, so a cyclic body given to assertz/1 or asserta/1 used to grow without end (review M7: `X = (a, X), assertz((p :- X))` reached 11.6 GB under `--heap-max 16`). A cyclic head or argument was already cut where it leads back, to an anonymous variable (`X = f(X), assertz(p(X))` stores `p(f(_))`). A or B?: A cuts a control construct that leads back to one being translated in the same way, to a goal that is a new variable, so the clause is finite and reaching that point raises `instantiation_error`; B makes assertz/1 raise an error for a cyclic body.

Decision (2026-11-03): A (review M7). `X = (a, X), assertz((p :- X))` stores `p :- a, call(_)` (clause/2 shows that body), `X = (Y = 1 ; X)` as the body of `p(Y)` gives `Y = 1` and then `instantiation_error`, each point that leads back gets its own variable, and a data argument that contains the construct is copied as data with its own cycle cut (`X = (q(X), true)` stores `q((q(_), true)), true`). Tests: `tests/pl/database.pl` `m7_assert_*`.

## 7.3.3 Evaluating a cyclic expression

The evaluator keeps the subterms it is working on in a stack of its own, outside the heap, so `X = X+1, Y is X` grew without end and past `--heap-max` (16 GB RSS in 10 seconds under `--heap-max 64`); since review L3, `format("~d", [X])` reached it too (review L20). 7.3.3 leaves cyclic terms undefined, and 7.9.1 evaluates an expression as a finite tree. A or B?: A detects the cycle and raises `resource_error(cyclic_term)` (7.12.2 h), which `catch/3` receives; B counts that stack against the heap limit, so the query stops there as the cyclic goals of "7.3.3 Calling a cyclic goal" do, and no `catch/3` receives it.

Decision (2026-11-06): A (review L20). Each frame of the evaluation's path is an argument of the frame below it, so a compound term met again on its own path means a cycle, and nothing else does. The evaluation watches one frame of its path and compares each compound term it enters with it, which is Brent's cycle detection on the path (`CycleWatch` in `src/builtins/arith.cpp`, one shared by the C++ recursion of the first 64 levels, `eval_at`, and the stack below them, `eval_deep`): the watch moves to the frame entered after 1, 2, 4, … entries, and to the parent when the frame it is on has been evaluated, side branch and all. An evaluation that never ends evaluates the same subterm the same way each time, so from some depth on the frames that stay on the path repeat with one period; the watch comes to rest on one of them, and once its window is longer than a period it meets the same term again within that period. So the work up to the error is a small multiple of the acyclic part and one period with its side branches, and a finite expression pays one comparison per compound term (no change in time measured: 1,000,000 rounds of two is/2 and one comparison take 0.97 s before and after, and 20 evaluations of a depth of 200,000 take 0.29 s before and 0.30 s after). `X = S+X` with a side branch `S` of depth 1,000,000 raises the error after evaluating `S` once (0.68 s, of which building `S` takes 0.66 s). The stack is at most about three times the path up to the first repeat, plus a side branch: outside `--heap-max` as before, but proportional to the expression. A period of 2^20 stops at 240 MB RSS and one of 2^20 + 1 at 365 MB, where evaluating the same expression without the cycle takes 155 MB. Arguments are still evaluated from the left, and a compound term is checked when it is entered, so an error met before the repeat comes first: `X = foo+X` raises `type_error(evaluable, foo/0)` and `X = _+X` raises `instantiation_error`, while `X = X+foo` and `X = X+_` raise `resource_error(cyclic_term)`. is/2, the comparisons, tab/1,2, the numeric arguments of format/2 (`~d`, `~e`, `~f`, `~c`, the column `~*` and the others that go through is/2 in `lib/format.pl`) and the code mosac generates all evaluate this way. Tests: `l20_*` in `tests/pl/arith.pl`, `tests/pl/lib_format.pl` and `tests/pl/utilities.pl` (23 do not stop within 1 second on the old code), `native_l20_*` in `tests/pl/native.pl`, and `is_cyclic`, `is_cyclic_after_deep`, `is_cyclic_side` and `is_cyclic_side_prefix` in `tests/stack/deep_builtins.pl`.

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

Decision (2026-10-29): ADR 0007. The texts above are the `ja` catalog's: a file that cannot be opened is the key `cannot_open_file` and a cycle is `file_cycle`, each with the path as argument. `messages_en.pl` gives the English texts. The decision above is otherwise unchanged.

## 7.4.2 Other directives (`:- G`)

7.4.2 lists the directives a standard-conforming text uses and SPEC section 3 item 8 lists the same set; until now any other `:- G` failed the load with `directive_unsupported`. Plan phase 6 Task 5 needs `:- write(x).` to run when the executable starts (test `mosac_emit_silent`), and the translator to skip such goals.

ISO (5.5) treats directives beyond 7.4.2 as an implementation-specific feature; a strictly conforming mode would have to reject them. No such mode is implemented.

Decision (2026-10-29, phase 6 Task 5, owner decided A on 2026-10-29): A directive whose goal is an atom or compound term, other than the names the loader handles (`include`, `ensure_loaded`, `use_module`, `module`, `export`, `dynamic`, `discontiguous`, `multifile`, `meta_predicate`, `initialization`, `char_conversion`, `set_prolog_flag`, `op`, `consult`, which keep `directive_unsupported` when their shape does not match), is called once at its position in the text, in the module being loaded, with the clauses read so far. Success continues the load. Failure fails the load with the key `directive_failed` and an uncaught exception fails it with the Formal, and in both cases the clauses and flags are undone like any other rejected directive. A variable or a number is still `directive_unsupported`. Tests: `general_directive_order` in `tests/pl/directives.pl`, `check_general_directive` in `tests/cpp/machine_test.cpp`.

`halt/0` and `halt/1` in a directive (this one or `initialization/1`) stop the load at once, and `mosa` and the `mosac` executable exit with that code; later directives, `initialization(_, main)` and `-g` do not run (`check_directive_halt` in `tests/cpp/session_test.cpp`, `mosac_emit_halt`). mosatest's per-directive fragment pass does not call these goals; the final load of the file calls them once.

Question asked: A (failure and exception undo the load, as above) or B (as SWI-Prolog, warn and continue)? The owner chose A on 2026-10-29.

## 7.4 Clauses for built-in predicates and control constructs, and bodies that are not callable

Review M10. consult adds a clause whose head is a built-in predicate or a control construct (`atom_length(_, 99).`, `length(_, 7).`, `call(_) :- write(mine).`, `catch(_, _, _).`, `(a, b) :- true.`, `! :- true.`) and a clause whose body is not callable (`foo :- 1.`, `foo :- (true, 1).`, `p :- (q ; 2).`) without an error, in mosa and in a mosac build (which loads the program with the same consult). The clause never runs (`atom_length(abc, L)` still gives 3), but `current_predicate(atom_length/2)` becomes true, and `foo` raises `type_error(callable, 1)` only when it is called, with context `call/1`. assertz/1 rejects the same clauses with `type_error(callable, Body)` and then `permission_error(modify, static_procedure, PI)` (8.9.1.3, in that order). A head that is a variable or a number already fails the load with `directive_unsupported`. Predicates written in Prolog under `lib/` (`append/3` and so on) are not built-in and stay redefinable; `foo :- X.` and `foo :- call(1).` stay valid (7.6.2). As we read 7.4 and 7.6, such a clause cannot be part of a Prolog text, but the text we have does not say how the processor reports it or whether it reads on; we have not checked the clause text itself. Every other load error in Mosa (`:- dynamic(true/0).`, clauses apart without `discontiguous/1`, a directive that fails or raises, a syntax error) fails the whole load and undoes it (7.4.2 Other directives above, owner decision A of 2026-10-29).

A or B?: A: such a clause fails the load with the Formal that assertz/1 gives for it (`type_error(callable, Body)` for a body that is not callable, else `permission_error(modify, static_procedure, PI)` for a built-in predicate or control construct), and the clauses and flags of the load are undone, like the other load errors. B: as SWI-Prolog, print that error as a warning, skip the clause, and read on. Not implemented before confirmation.

Decision (2026-11-03): A (review M10). The loader checks each clause after translating it and before adding it (`refused_clause` in `src/machine/load_clauses.cpp`, called from `take_term`), in the order of assertz/1: a body with a number in goal position gives `type_error(callable, Body)`, and then a head that is a built-in predicate, a control construct, `:-/1` or `:-/2` gives `permission_error(modify, static_procedure, Name/Arity)`. This holds in every module and with a `user:` qualifier, since SPEC Section 5 puts the built-in predicates and control constructs in `system`. The load fails with that Formal as its diagnostic and its clauses and flags are undone; consult/1 called at run time fails, and `mosac emit` and `mosac build` stop without writing C++. A translation load does not call general directives, so after one of them (whose op/3 may change how later terms read) a refused clause is treated like a syntax error there: it is left out, the file runs as IR, and the executable's own load at start-up decides, with the same diagnostic as mosa (Codex review). Tests: `m10_*` in `tests/pl/directives.pl`, `check_refused_clauses` in `tests/cpp/machine_test.cpp`, ctest `mosac_emit_refused` and `mosac_emit_refused_op`.

## 7.11 Flag debug

Decision (2026-10-25): Open question 9: A. The default is `off`. Even when set to `on`, Phase 3 does not trace.

## 7.11 Flag unknown: warning

Decision (2026-10-25): Open question 10: B. `unknown` accepts only `error` and `fail`. `warning` is not accepted.

## 7.11 Flags float_overflow, float_zero_div, float_undefined

Decision (2026-10-25): Open question 11: B. `float_overflow`, `float_zero_div`, and `float_undefined` accept only `error`. No other atom names are introduced.

## Section 4 Flag invalid_utf8

Decision (2026-10-25): Open question 12: A. The default of `invalid_utf8` is `error`. With `replace`, U+FFFD is inserted.

## Section 4 and 8.12.1 Position after an invalid UTF-8 sequence

SPEC Section 4 makes an invalid UTF-8 sequence raise `representation_error(character)` when `invalid_utf8` is `error`, but does not say where the stream stands afterwards, and 8.12.1 and 8.14.1 do not either. Mosa left the position before the sequence, so a loop that catches the error and reads on raised the same error forever, for get_char/1,2 and for read/1 (review L6). Independently of this, `replace` turned only the first byte into U+FFFD, so a sequence cut short such as `E3 81` became two U+FFFD.

A or B?: A: advance past the sequence as one character, then raise, so the position is the one `replace` would leave; peek_char/1,2 does not advance. B: keep the position before the sequence.

Decision (2026-11-05): A. In both modes the sequence is its maximal subpart (Unicode 15.0, Section 3.9, Table 3-7: the longest prefix of a well-formed sequence, at least one byte), so `E3 81` is one character and `ED A0 80` is three. read/1 reads on as `replace` would, with U+FFFD in place of each sequence, to the end of the term (or past a syntax error, as for any syntax error), then puts the original bytes back and raises; the term is dropped, so `foo(<E3>).` followed by `bar.` gives `representation_error(character)` and then `bar`. A sequence in a comment belongs to the read of the term after it, which is dropped too (`% <E9>` then `r.` and `ok.` gives the error and then `ok`). A leading byte order mark is skipped when the position of the sequence is worked out, as the lexer skips it. Tests: `l6_*` in `tests/pl/chars.pl`, `tests/cpp/stream_test.cpp` ("utf flag advances" and "utf error advances" in `check_invalid_utf8`, "read utf advances" in `check_read_term`, and "peek code stays on bad utf" in `check_get_code`, whose file now has two bad bytes; before, they expected the stream to stay before the bad byte).

## Section 4 Flag japanese_punctuation

Decision (2026-10-25): Open question 13: A. `set_prolog_flag(japanese_punctuation, true)` stores `true` as the value. Lexical analysis is not changed.

Decision (2026-10-28): phase 5 applies the flag to lexical analysis (SPEC Section 4 table). Outside quoted tokens, comments and `0'c`, `（）［］，、：` read as `()[],,:`. `。` is a clause terminator only when a layout character or the end of input follows; otherwise it stays a symbol character, so `1。5` is not a float. `「` opens a quoted atom that `」` closes. The flag is per file (SPEC Section 5) and takes effect from the clause after the directive.

Decision (2026-10-31): review H1 follow-up: A. `。` is also a clause terminator when `%` follows, the same as `.` (6.4.8, SPEC Section 4 table: `。` reads as `.`), so `w（x）。% c` reads `w(x)`.

Interpretation (2026-11-03, review M8): "`writeq/1` always outputs the half-width form" covers the punctuation the writer emits itself. The atom `'、'` is not the atom `','`, so 7.10.5 h applies: with `quoted(true)` an atom is quoted when it could not be read back as that atom without quotes. While the flag is true, a symbol-character atom that contains `（` `）` `，` `、` or `：`, or ends in `。` (a layout character after it would end the clause), is quoted: `writeq(f('、'))` writes `f('、')` and `writeq('：'(a))` writes `'：'(a)`. A `。` inside such an atom is followed by a symbol character, so `。+` stays bare; `［］「」` were quoted already. With the flag false these characters are symbol characters and stay bare (`f(、)`). Every quoted write (writeq, write_canonical, write_term with `quoted(true)`, format's `~q`), the toplevel's answers, message arguments (written as text and read again by the catalog query), and the REPL, which writes each goal it read and reads it again, use the flag's value at the time of writing. Rejected: quoting these characters whatever the flag, so that output reads back under either value; 7.10.5 h quotes only what the current reader needs, and default-mode output such as `f(、)` would change.

## Section 7 Flag message_language

Decision (2026-10-28): ADR 0007 (owner approved). An implementation-defined flag (7.11.3) taking `ja` or `en`, changeable; any other atom gives `domain_error(flag_value, message_language+V)`. The default is `ja` when `LANG` starts with `ja`, else `en`. It chooses only the catalog of the top-level messages and does not change any error term.

## 7.11 Flag version

Decision (2026-10-25): Open question 14: A. `version` is a read-only atom, and its text is the same as `version_line()`.

Superseded: the implemented flag is the read-only integer Major*10000 + Minor*100 + Patch (SPEC section 3 item 7; `src/builtins/flags.cpp`), `200` in mosa 0.2.0. The atom form above was never implemented.

## Section 3 Flag iso

Decision (2026-10-25): Open question 15: A. The default of `iso` is `true`, and it is read-only. Phase 3 adds no new behavior. Implemented after phase 10 (`src/builtins/flags.cpp`, `tests/pl/flags.pl`): `current_prolog_flag(iso, true)` holds and `set_prolog_flag(iso, _)` gives `permission_error(modify, flag, iso)` for `true` and `false`, and `domain_error(flag_value, iso+_)` for any other value.

Decision (2026-11-13, the Muse review's L13; owner confirmed): A. SPEC section 3 used to say the intentional deviations are "disabled when the `iso` flag is true", which cannot hold for a read-only `true` flag with no second mode; commit 2723dc6 aligned the sentence to the implementation (deviations behind flags that default off, or visible only after an explicit import) without recording the question here first. Mosa has no strict-conformance mode: the read-only `true` flag only states that the core semantics follow ISO. The repository still holds no text of 13211-1 5.1 e, so this decision does not by itself settle whether that clause would require such a mode; revisit B (a writable mode that turns the deviations off) if external certification or a user need for rejecting extensions arises.

## 8.12 Newlines in text

Decision (2026-10-25): Open question 16: A. In text files and memory streams alike, on every OS, a newline is a single U+000A. It is not left to C++ text mode.

## 8.11 Stream terms

Decision (2026-10-25): A stream term is always `'$stream'(N)`. `N` is the index into the stream vector. An alias is a separate name and does not change the form of the term.

## 8.11 Standard streams

Decision (2026-10-25): Index 0 is `user_input` (stdin), 1 is `user_output` (stdout), 2 is `user_error` (stderr). `reposition` is false.

## 8.11 Reading past end of stream

Decision (2026-10-25): The default `eof_action` is `eof_code`. Reading past the end gives `end_of_file` for text and `-1` for binary. `eof_action(reset)` can be chosen separately.

Since 2026-10-28, `user_input` is the exception and has `eof_action(reset)` (see "8.11 Properties of the standard streams" below).

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

## Conformance suite: Logtalk message sending in test bodies

Recorded (2026-11-13, whole-review item 3): five failing rows send a Logtalk message with `::/2` in the test body: `lgt_soft_cut_2_3_22` (`lgtunit::deterministic/2`), `logical_update_semantics_negation` (`integer::between/3`), and `lgt_open_4_41` to `43` (`os::ensure_file/1`). `::/2` is Logtalk object syntax; ISO/IEC 13211 has module qualification `:/2` (SPEC section 5) and no objects. The runner executes test bodies as Prolog goals (with `^^` super calls served by `src/machine/lgt_io.cpp`) and does not implement message sending, so each row ends in `existence_error(procedure, ::/2)` before its subject runs. The subjects themselves are covered elsewhere: `*->/2` (SPEC section 7 stage A, non-ISO), the logical-update view of 7.5.4 ("7.5.4 Open scans and retract" below), and repeated `eof_action/1` options of `open/4` (7.10.2.11; "8.14 Write options" records that the later option wins). The five rows stay failing as a limit of the runner, not as a difference in Prolog semantics.

## Conformance suite: os::operating_system_type/1 in a condition

Recorded (2026-11-13, whole-review item 3): `lgt_open_4_31` (8.11.5 `open/4`) carries the condition `os::operating_system_type(unix)`. The condition evaluator (`src/machine/lgt_cond.cpp` `message_cond`) answers only the `windows` form (as not holding, which puts a row in the side column); any other operating-system name is `Bad`, and `apply_condition` (`src/machine/lgt_file.cpp`) then counts the row as failing with actual `unsupported`. The `errors([...])` outcome itself is supported (`LgtKind::Errors`), but the body never runs. The row stays failing as a limit of the runner: it does not determine an operating-system type for conditions.

## Conformance suite: ignore/1 in lgt_catch_3_13

Recorded (2026-11-13, whole-review item 3): `lgt_catch_3_13` (7.8.9 `catch/3`) calls `ignore({q})`. `ignore/1` is not in ISO/IEC 13211, is not among the non-ISO additions of SPEC section 3, and none of the six libraries the runner imports ("Section 5 Autoload") defines it, so the row ends in `existence_error(procedure, ignore/1)`. The row stays failing: `ignore/1` is outside the implemented scope, and `catch/3` itself is covered by the other rows of `control/catch_3` and by `tests/pl/control.pl`.

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

Since phase 11, the `sort_2` and `keysort_2` directories of the bundled suite count in the ISO column of `docs/conformance.md` (`kIsoExtraScope` in `src/machine/lgt_suite.hpp`). The only rows of theirs left in the separate section are cases the runner sets aside, as in other directories (for example `iso_sort_2_04`, inside a conditional-compilation branch).

## Section 7 forall/2

`forall/2` is listed in stage G of SPEC section 7 ("Non-ISO utilities (`library(*)`, the table in section 5)"), and section 5 names it among the predicates that must work across modules, but none of the six modules in the section 5 table exports it.

A or B?: A: implement `forall/2` in `system` as a meta predicate in phase 4 Task 9. B: do not implement it in phase 4. Not implemented until confirmed.

Decision (2026-10-28): A. `forall/2` is implemented in `system` as a meta predicate in phase 4 Task 9, as `\+ (Cond, \+ Action)` run in the caller's context module. It is the only stage G predicate outside the six modules that phase 4 adds.

## Section 7 Stage G utilities

Review M11. Stage G of SPEC section 7 lists `tab/1,2`, `term_to_atom/2`, `nb_setval/2`, `nb_getval/2`, `setup_call_cleanup/3`, `sort/4` and `numbervars/3` (written `number_vars/3` there), but no phase in SPEC section 9 names them, phase 4 added only the six modules of the section 5 table and `forall/2`, and none of them exists, so each call raises `existence_error`.

A or B?: A: implement them in phase 7, before the manuals and the release packages, in `system` as `forall/2` is, with the argument modes and errors of SWI-Prolog since none is ISO; `setup_call_cleanup/3` needs the engine to run the cleanup on exit, failure, cut and exception, so ADR 0016 comes first; SPEC's `number_vars/3` becomes `numbervars/3`. B: leave them out of this release (the existence error stays), remove them from stage G and say so in the manuals. Not implemented before confirmation.

Decision (2026-11-03): A (review M11). The seven predicates go into `system` in phase 7, one predicate per commit, with their tests in `tests/pl/utilities.pl`; where SWI-Prolog's behaviour is not visible from its manual and source, the choice is written in the header comment of the predicate's file. SPEC section 7 now says `numbervars/3`.

Implemented 2026-11-03 (`setup_call_cleanup/3` under ADR 0016). Where Mosa departs from SWI-Prolog: `nb_getval/2` gives a fresh copy on each call, since the heap is reclaimed on backtracking; `sort/4` takes only an integer Key and gives `type_error(sort_key, Key)` for SWI's dict keys and key paths, since Mosa has no dicts; `term_to_atom/2` reads `[]` as the atom `'[]'` and gives `syntax_error(incomplete_term)` for empty or unfinished text (SWI 9.3.5 and later: `end_of_string`); `numbervars/3` has no upper bound since phase 10: a number past the 64-bit integers is a Big (phase 7 to 9 gave `representation_error(max_integer)` there; SWI: `representation_error(tagged_integer)`), and since phase 11 it raises `type_error(integer, End)` when End is neither an integer nor a variable, as the bundled suite's `commons_numbervars_3_15` expects (SWI-Prolog 10.0.2 fails there, as Mosa did before); `setup_call_cleanup/3` runs no cleanup on `halt` or a heap-limit stop, drops the cleanup's exception when the goal raised (SWI keeps the more urgent one), runs every cleanup removed by one cut and then rethrows the first of their exceptions, and ignores a cleanup's exception when a new query of the embedding API closes the old one (a `-g` goal, a toplevel answer and `initialization(G, main)` are cut after their answer as in SWI, and their cleanup's exception is the goal's).

`tab/1,2` with a count beyond int64 (phase 10, design section 8). A positive Big gets what a too-large int64 gets, and `tab(1 << 40)` writes that many spaces. So since phase 10 task 8, a positive Big writes spaces without end (`run_tab` in `src/builtins/chars.cpp`). A negative Big writes nothing, as a negative count does (`tab_big_neg` in `tests/pl/bigint.pl`). Before task 8, a Big gave `evaluation_error(int_overflow)`; SWI-Prolog gives that for a Big of either sign.

A or B?: A: keep the endless output for a positive Big, the same as a too-large int64. This is the default and the current behaviour. Its cost is that inside `with_output_to/2`, or on any other memory stream, the output grows outside the heap limit: `tab(1 << 40)` there reached 11 GB RSS in 15 s. The process therefore runs out of memory instead of stopping at `--heap-max`. B: give an error for a Big of either sign, such as SWI-Prolog's `evaluation_error(int_overflow)`; this is a one-line change in `run_tab`.

Decision (2026-11-13): A (owner decision A). B is rejected. A positive Big count writes spaces without end, the same as a too-large int64, and a negative Big writes nothing.

## ISO/IEC 13211-2 7.2.2 predicate_property/2

SPEC section 5 lists `predicate_property/2` in stage F but does not say what a variable head or an unknown module gives.

A or B?: A: a variable head gives `instantiation_error`, as in the 13211-2 DTR and the bundled suite (`commons_predicate_property_2_16`); an unknown `M:` gives `existence_error(module, M)`, the same as `M:G`. B: enumerate the visible predicates, as SWI-Prolog does, and fail for an unknown module.

Decision (2026-10-28): A for both. A built-in has `defined`, `built_in` and `static`, and a control construct or goal-taking built-in also has `meta_predicate(Head)` (for example `findall(?, 0, -)`). A user predicate is looked up as a call would be and has, in this order, `defined`, `dynamic` or `static`, `exported`, `imported_from(M)` (only through an explicit import of the context module), `multifile`, `meta_predicate(Head)` (when declared) and `number_of_clauses(N)`. Any other property term gives `domain_error(predicate_property, P)`.

## Section 5 Operators in modules

SPEC section 5 makes `op/3` local to the declaring module, lets `op(P, T, N)` in an export list reach importers, and makes `user` operators visible everywhere. It does not say what `op(0, T, N)` does inside a module, or whether an import list `[PI, ...]` brings exported operators.

A or B?: A: a local `op(0, T, N)` hides the `user` and ISO core operator of that class inside the module only, as in SWI-Prolog; exported operators come in with `use_module/1`, `except(...)` and a consulted module file, but not with an import list. B: a local `op(0, T, N)` removes only the module's own entry; exported operators come in with every form of import.

Decision (2026-10-28): A for both. Lookup is the module's own layer, then the `user` table (which holds the ISO core). `op/3`, `current_op/3`, `read_term/2,3` and the `write` family use the caller's context module. A `:- op` directive in a module file defines into that module. Importing into `user` puts the operator in the `user` table, so every module sees it.

Amendment (2026-11-06, owner decision A on review L19 A1, ADR 0006 amendment 1): while a file found through `library(...)` is read, the ISO core stands in for the `user` table, so the file's own layer and the core read it; its `user:op/3` goes into both the core and the set-aside `user` table at once, and is refused if either refuses it.

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

## Section 4 format/2 fill characters by width

SPEC section 4 counts `~t~|` columns by East Asian width. A fill character of width 2 (``~`漢t``) cannot always fill an odd number of columns exactly: ``~`漢t~5|`` leaves one column after two `漢`.

A or B?: A: fill with as many fill characters as fit and pad the remaining columns with spaces, so the column stop is kept. B: add one more fill character and overshoot the column stop by one.

Decision (2026-10-29): A. The column stop is what `~|` promises, and SPEC section 4 measures it by width. A fill character of width 0 counts as width 1 so the fill always ends.

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

Decision (2026-10-28): B. `if/3` is a built-in in `system`, listed next to `*->/2` in stage A of SPEC Section 7. `if(C, T, E)` runs `(C *-> T ; E)` in the caller's context module; like `call/1`, a cut in any argument is local, and an argument that cannot be called gives `type_error(callable, if(C, T, E))` before anything runs (`src/builtins/control.cpp` `run_if`, `tests/pl/control.pl` `if_*`).

## Section 3 Flags dialect and version_data

The flags `dialect` and `version_data` are not in 7.11 or in SPEC Section 3. `commons_current_prolog_flag_2_16` to `18` get `domain_error(prolog_flag, dialect)` and `domain_error(prolog_flag, version_data)`.

A or B?: A: outside SPEC, so they are not implemented, and the 3 rows stay failing. B: add `dialect` (`mosa`) and `version_data` (`mosa(Major, Minor, Patch, [])`) to SPEC Section 3 and implement them. A until confirmed.

Decision (2026-10-28): B. Both flags are read-only and listed in SPEC Section 3 item 7. `version_data` takes its numbers from `include/mosa/version.hpp`, which `mosa --version` also uses. Setting either gives `permission_error(modify, flag, F)` when the value has the right shape (an atom, or a compound of arity 4) and `domain_error(flag_value, F+V)` otherwise (`src/builtins/flags.cpp`, `tests/pl/flags.pl` `flag_dialect*`, `flag_version_data*`).

## Section 4 Stream encoding and BOM

The 21 failing rows in `unicode/builtins` expect the IANA name `'UTF-8'`, a BOM, and no encoding on binary streams. SPEC Section 4 and Section 3 item 7 already fix the flag `encoding` to `utf8` and give every stream an `encoding` option that defaults to `utf8`, and Mosa follows that: `current_prolog_flag(encoding, utf8)`, `open/4` accepts `encoding(utf8)`, and binary streams also report `encoding(utf8)`. So `open/4` with `encoding('UTF-8')` gives `domain_error(stream_option, encoding('UTF-8'))`, `lgt_unicode_current_prolog_flag_2_02` fails on the name, and `lgt_unicode_stream_property_2_07` and `09` fail because a binary stream reports `encoding/1`. The `open/4` option `bom/1`, the stream property `bom/1`, and skipping a BOM when a text stream is read are in neither the standard (7.10.2) nor SPEC.

A or B?: A: keep SPEC as it is (the name `utf8` only, every stream reports `encoding/1`, no BOM handling), and the 21 rows stay failing. B: amend SPEC Section 4 to accept `'UTF-8'` as another name, drop `encoding/1` from binary streams, add the `bom/1` option and property, and skip a BOM on text input, then implement them. A until confirmed.

Decision (2026-10-28): A. These options, properties, and names are outside the standard, and SPEC already fixes the encoding to `utf8` on every stream. The 21 rows stay failing as a difference in interpretation. Skipping a BOM on text input may be asked separately in Task 15.

## 9.3.10.3 (^)/2 with a negative integer exponent

`iso_integer_power_2_06` expects `2^(-1)` to give `type_error(float, 2)`. `src/builtins/arith.cpp` reads 9.3.10.3 e of Corrigendum 2 as giving the float type error only when the exponent is less than -1, and gives `evaluation_error(undefined)` for `2^(-1)`.

A or B?: A: keep the current reading (`evaluation_error(undefined)`). B: any integer base other than 1 and -1 with a negative integer exponent gives `type_error(float, Base)`. A until confirmed.

Decision (2026-10-28): B. An integer base other than 0, 1 and -1 with a negative integer exponent gives `type_error(float, Base)`, so `2^(-1)` does too. `1^N` is 1, `(-1)^N` is 1 or -1 by the parity of `N`, and `0^N` stays `evaluation_error(undefined)` (9.3.10.3 d, as in the 9.3.1.3 d decision). `hat_neg` in `tests/pl/arith.pl` changes from `undefined` to the type error in the same commit (`src/builtins/arith.cpp` `negative_hat`).

## 8.11 Properties of the standard streams

`sics_stream_property_2_05c` and `lgt_stream_property_2_08b` expect `user_input` to report `eof_action(reset)`. `sics_stream_property_2_06d`, `lgt_stream_property_2_09c`, and `lgt_stream_property_2_10c` expect `user_output` and `user_error` to report `mode(append)`. Mosa reports `eof_action(eof_code)` (the default in "8.11 Reading past end of stream") and `mode(write)`.

A or B?: A: keep `eof_code` and `write`. B: `user_input` has `eof_action(reset)`, and `user_output` and `user_error` have `mode(append)`. A until confirmed.

Decision (2026-10-28): B. `user_input` reports `eof_action(reset)` and acts on it: after the end, the next read clears the end mark and asks the OS again, without replaying what was already read, so a terminal can read on after Ctrl-D and a pipe keeps giving `end_of_file`. `user_output` and `user_error` report `mode(append)`. `all_std` in `tests/pl/stream_prop.pl` and `capture_leaves_output` in `tests/pl/write.pl` change in the same commit (`src/builtins/streams.cpp` `standard_stream`, `src/builtins/chars.cpp` `reset_at_end`, `tests/cpp/stream_test.cpp` "standard reset").

## Spec Section 4: So characters are symbol characters

`lgt_unicode_escape_sequence_full_05` expects `writeq('🀙🀚🀛🀜🀝🀞🀟🀠')` to quote the atom. SPEC Section 4 makes non-ASCII characters in `So` symbol characters, so these 8 characters form a symbol-character atom (6.4.2), and `writeq` (7.10.5) writes it without quotes. The row stays failing as a difference in interpretation. This follows the approved SPEC, so no question is open.

## 7.10.5 a Writing a variable that occurs once

`lgt_write_term_3_043`, `044`, `065`, and `066` expect a written variable to be `_` followed by at least one more character. Mosa writes a variable that occurs once in the term as `_` alone, and a variable that occurs more than once as `_G` followed by a number (`f(_G1,_G1,_)`). 7.10.5 a says only that the text of a variable is implementation dependent and begins with `_`.

A or B?: A: keep `_` for a variable that occurs once (the current writer). B: give every variable a name with at least one character after `_`, such as `_G1`. A until confirmed.

Decision (2026-10-28): B. The writer names every unnamed variable `_G` and a number in order of appearance, starting at 1 in each call (`f(_G1,_G2,_G1)`). The toplevel writes the bindings of one answer as one call (`write_terms`, review M22). The conformance runner writes terms to text and reads them back pasted together, and relies on a variable that occurs once being fresh on each reading, so it sets the new `WriteOptions::underscore_once` and keeps `_` for those. The roundtrip check in `src/read/roundtrip.cpp` treats names the writer made up as anonymous. `write_fresh` in `tests/pl/write.pl` and "single unnamed" in `tests/cpp/writer_test.cpp` change in the same commit (`src/write/writer.cpp` `var_text`, `src/machine/lgt_suite.hpp` `lgt_write_options`, `tests/cpp/suite_vars_test.cpp` `fresh_*`).

## 9.1.6 Rounding functions given an integer

`lgt_ceiling_1_05`, `lgt_floor_1_06`, `lgt_round_1_13`, `lgt_truncate_1_06`, `lgt_float_integer_part_1_04`, and `lgt_float_fractional_part_1_04` expect `X is floor(9)` and the like to give `type_error(float, 9)`. 9.1.6 gives `floor`, `ceiling`, `round`, and `truncate` the signature F → I, and `float_integer_part` and `float_fractional_part` the signature F → F, so an integer argument is a type error. Mosa returns the integer for the first four (`floor(9) = 9`) and a float for the last two (`float_integer_part(9) = 9.0`). SPEC lists these evaluables but says nothing about their argument types.

A or B?: A: follow 9.1.6 and give `type_error(float, N)` for an integer argument to all six. B: keep accepting integers (the current behaviour). B until confirmed.

Decision (2026-10-28): A. An integer argument to these six gives `type_error(float, N)`, where `N` is the evaluated value (`floor(4 + 5)` gives `type_error(float, 9)`). `integer/1` is not one of them and still accepts integers (`src/builtins/arith.cpp` `float_only`, `tests/pl/arith.pl`). The `iso` flag is read-only `true`, so this is the only mode.

## 9.1.6 round/1 of a negative half

`eclipse_round_1_07` and `08` expect `round(-3.5)` to be `-3` and `round(-4.5)` to be `-4`. 9.1.6 defines `round(x)` as `floor(x + 1/2)`, which rounds a half towards positive infinity. Mosa uses `std::round`, which rounds a half away from zero, so it gives `-4` and `-5`. Positive halves give the same result either way (`round(2.5) = 3`).

A or B?: A: keep rounding a half away from zero (`round(-2.5) = -3`). B: follow 9.1.6, `floor(x + 1/2)` (`round(-2.5) = -2`). A until confirmed.

Decision (2026-10-28): B. `round/1` follows 9.1.6 and rounds a half towards positive infinity. The fractional part is taken by subtracting `floor(x)`, which is exact, so `0.49999999999999994` and odd integers above 2^52 are not rounded twice (`src/builtins/arith_float.cpp`, `tests/pl/arith.pl` `round_*`).

## Section 4 Inside 「…」 quoted atoms

SPEC Section 4 maps `「` `」` to the start and end of a quoted atom and says they cannot be nested. It does not say which characters inside are special.

A or B?: A: the same rules as `'…'` (6.4.2): `\` escapes work, `」」` is one `」`, a raw newline is an error, and `'` is an ordinary character. B: no escapes; every character up to the first `」` is taken as written, and `」` cannot appear inside.

Decision (2026-10-28): A. `「…」` uses the escapes of `'…'`, with `」` as the quote character, so `」」` is one `」` and `'` is ordinary.

## 7.7.7 and SPEC Section 5: statically undefined calls in `mosac emit`

ISO has no translation step: calling a procedure that is defined nowhere is an `existence_error(procedure, PI)` when the goal runs (7.7.7, flag `unknown` 7.11.2.4, 7.12.2 c), and SPEC Section 5 gives `existence_error(module, M)` at run time for `M:G` with an unknown `M`. `mosac emit` (phase 6 Tasks 5 and 7) instead rejected the program at translation time when a clause body names, literally, a predicate with no definition anywhere, including `nomod:foo(X)` with a module that is never loaded (`mosac_undefined_call`, src/codegen/check.cpp), and wrote nothing, until the decision of 2026-11-05 below made this a warning.

Decision (2026-10-29): this is a deliberate deviation of the translator only. The interpreter and the generated executable keep the ISO behaviour at run time: the error is raised when the goal is reached (`tests/pl/modules.pl` `mod_qual_unknown`; `tests/pl/native.pl` `native_qualified_missing`, which reaches the unknown module through a variable `M` so that the translation is accepted). `mosatest --diff` (`--units`, Task 11) skips the check so that tests of existence errors can be translated.

Decision (2026-10-30): A. The diagnostic suggests `:- dynamic(Name/Arity).` for a program that creates the procedure at run time with `assertz/1`. The suggestion is written as `dynamic(Name/Arity)` and not as the prefix form `dynamic Name/Arity`, because Table 7 (6.3.4.4) has no `dynamic` operator, and 7.4.2.1 `dynamic/1` takes the predicate indicator as an ordinary argument. The name is written as `writeq` writes it, in brackets when it is an operator or a symbol-character atom (ctest `mosac_emit_undefined_quoted`).

A or B? (review L12): a program that handles such calls on purpose, with `unknown` set to `fail` or by catching `existence_error`, was rejected by mosac although it runs under mosa, and the hinted `:- dynamic` changes what a catch sees (the call fails instead of raising). A: print the same diagnostic as a warning and still build; B: keep stopping.

Decision (2026-11-05): A. `mosac emit`, `build` and `run` print the diagnostic (file and line, with the `:- dynamic` hint) prefixed by "Warning:" and go on; the executable raises the error or follows the `unknown` flag when the goal runs, as mosa does. Export inconsistencies still stop the translation. Tests: ctest `mosac_emit_undefined`, `mosac_emit_undefined_catch`, `mosac_emit_undefined_quoted` (these now build and run) and `mosac_emit_undefined_handled`.

## 7.1 Nesting depth of terms (implementation limit)

The writer, the loader (clause building, `IrTerm` copy and destruction, materialization), unification and the builtins that walk terms (`copy_term/2`, `findall/3`, `bagof/3`, `setof/3`, `compare/3`, `==`, `term_variables/2`, `clause/2`, `unify_with_occurs_check/2`, `is_list/1`) keep deep subterms on explicit stacks (ADR 0011). Under a 1 MiB stack they handle a term nested 1,000,000 deep and a 200,000-element list literal (ctest `stack_deep_write`, `stack_deep_builtins`, `long_literal`). Goals and arithmetic expressions built at run time (`call/1`, `assertz/1`, `is/2` and the comparisons) have no depth limit either (ADR 0011 amendments 2 and 3): expressions and left- and right-nested `,` `;` `->` `*->` `catch/3` nested 1,000,000 deep are checked under a 1 MiB stack, and run in time linear in their length.

Nested `findall/3`, `findall/4`, `bagof/3` and `setof/3` run inside the caller's loop and do not use the native stack (ADR 0013, review H4): 100,000 levels under a 1 MiB stack in mosa and in a mosac executable (ctest `stack_deep_collect`, `mosac_deep_collect`). Portray hooks, `with_output_to/2` and directives still start a nested `run_loop` per level, so nesting them inside each other, or interleaving them with a collection, is bounded by the native stack: `w(N) :- M is N-1, findall(x, with_output_to(atom(_), w(M)), [x]).` passes 500 levels and ends in SIGSEGV at 1,000 under 1 MiB.

Phase 9 made the reader iterative too (ADR 0011 amendment 4): it reads with one loop over an explicit stack that holds one frame per nesting level of prefix and infix operators, arguments, lists, parentheses and curly braces. Under a 1 MiB stack it reads `f(f(...))`, `[[...]]`, nested prefix operators, right-nested `xfy` operands, nested parentheses, nested curly braces and nested `|` tails 1,000,000 levels deep (ctest `stack_deep_read`), and a program built with `mosac build` reads a term nested 100,000 deep from a data file (ctest `mosac_deep_file`). `mosac build` of a source file with a clause nested 100,000 deep also succeeds and the program runs under a 1 MiB stack, but clang took about 80 minutes and about 1.2 GB of memory to compile the 12 MB generated unit (macOS, 2026-11-11). The owner treats nesting depth as an implementation limit (ISO 7.1, implementation defined) and not as a deviation from a requirement.

Decision (2026-10-30): A. Make the writer, the reader and the loader iterative in phase 7 or later. Phase 7 made the writer and the loader iterative (ADR 0011), and `mosac_long_list` runs 20000 elements on every OS. Phase 9: the reader (ctest `stack_deep_read`).

## 7.1 No garbage collection of the atom table (implementation limit)

The GC of ADR 0010 reclaims the heap, `extras` and the trail, but not the atom table, so a loop that keeps creating new atoms (for example with `atom_concat/3` or `atom_codes/2`) makes the table grow until the process ends. This is a known limit of the implementation, not a deviation from the standard.

Decision (2026-10-30): A. Keep the limit through phase 7; ADR 0010 lists atom table GC as a non-goal.

## 8.11.6.1 close/1,2 of a standard stream

Before the decision below, Mosa really closed `user_input`, `user_output` and `user_error` (`src/builtins/streams.cpp` `run_close`, `mark_closed`). After `close(user_output)` the current output falls back to stream 1, which is closed, so every later `write/1` raises `existence_error(stream, user_output)` for the rest of the session (`mosa -g "close(user_output), write(hello)"`); `with_output_to/2` and `query()` land on the same closed stream (review item L13, found in the H6 review). The text of 8.11.6.1 was not available while writing this note. The Logtalk suite only expects `close(user_input)`, `close(user_output)`, `close(user_error)` and their `force(true)` forms to succeed (`lgt_close_1_07`..`09`, `lgt_close_2_16`..`18`); it does not check later output. Prolog-cafe returns from close/2 without closing a standard stream.

A or B?: A: close/1,2 of a standard stream flushes it and succeeds without closing it, so it stays the current stream and stays writable. B: keep closing it (current behaviour). B until confirmed.

Decision (2026-11-01): A. close/1,2 checks its arguments and options as before, flushes `user_output` and `user_error`, and leaves the three standard streams open (`src/builtins/streams.cpp` `run_close`, `tests/pl/close.pl` `close_std_*`, `tests/cpp/output_stop_test.cpp` `check_session_close_user_output`). `tests/cpp/stream_test.cpp` changes in the same commit: "close user_output" now expects the stream to stay open and current, and "closed current input" (now "closed input") closes a file stream instead of `user_input`.

## 8.11 Reading a whole file at open and consult

`open(File, read, S)` read the whole file into memory before it succeeded, and `consult` did the same with the source text, so `open('/dev/zero', read, S)` alone grew without bound (whole-review item 2). The owner chose B of the ways put to them (2026-11-13): a file opened for reading is read in chunks as the reading predicates need it (`pull_stream` in `src/builtins/streams.cpp`), keeping what was read so `set_stream_position/2` and `eof_action(reset)` work as before, and the bytes kept count against the `--heap-max` budget (`heap_byte_budget` in `include/mosa/heap.hpp`), stopping with the heap-limit stop past it. What another program appends to the file later becomes readable, where the old snapshot did not show it. `consult` and the other loads take a regular file only, under the same byte budget (`read_source` in `src/machine/load_path.cpp`); a device or pipe is `cannot_open_file`. Opening for append still reads the text that is already there (the position and saving machinery needs it), so it follows the consult rule: a regular file only, under the budget, and anything else is `permission_error(open, source_sink, File)`. Tests: `check_lazy_read` and `check_consult_source_limit` in `tests/cpp/stream_test.cpp`.

## 6.4.5 Float literal out of range

Before the decision below, a float token whose value does not fit in an IEEE 754 double (`1.0e400`, `1.8e308`) silently became `inf.0` (`src/read/lexer_literal.cpp` `make_float`), in read/1, consult, number_codes/2, number_chars/2 and atom_number/2 alike. `X is 1.0e400 * 1` therefore did not raise although `float_overflow` is `error` (7.11), and `inf.0` does not read back, so 8.16.8 round-trip failed (review item M2). The standard does not say what an out-of-range float token is.

A or B?: A: `syntax_error(float_overflow)`, the same way as `syntax_error(integer_overflow)` for integers (read/1 skips to the full stop). B: `evaluation_error(float_overflow)`.

Decision (2026-11-02): A. The lexer reports the new ADR 0007 key `float_overflow` for a float above the largest double; a spelling that rounds to the largest double is still a number. The direction of an out-of-range value is taken from its magnitude (the place of the first non-zero digit plus the exponent), not from the sign of the exponent, so `1<400 zeros>.0e-10` overflows and `0.<400 zeros>1` underflows. Underflow is unchanged: `1.0e-400` reads as `0.0` and subnormals such as `1.0e-310` are kept (`tests/pl/read.pl` and `tests/pl/atoms.pl` `m2_*`, `tests/cpp/lexer_test.cpp` `test_float_range`).

Since phase 10 (2026-11-12, ADR 0025), an integer token has no size limit, so `syntax_error(integer_overflow)`, which option A took as its model, no longer exists; a float token out of range still gives `syntax_error(float_overflow)`.

## 6.4.5 NaN and infinity (non-ISO)

Before the decision below, writeq/1 and the other writers spelt a NaN or an infinity with the text of `std::to_chars` plus `.0` (`src/write/spelling.cpp` `format_float`): `nan.0`, `inf.0` and `-inf.0`, and for a NaN with its sign bit set the standard library's own text, `-nan(ind).0` on macOS (`X is -(nan)`, or `sign(-(nan))` since L2). None of them read back: `term_to_atom(T, '-nan(ind).0')` and `term_to_atom(T, 'nan.0')` raise `syntax_error(unexpected_token)` (review L22). ISO floats have no NaN and no infinity, so the standard says nothing about their text; SWI-Prolog and ECLiPSe write `1.0Inf`, `-1.0Inf` and `1.5NaN`, after Joachim Schimpf's proposal for a core update on floating point, and read them back.

A or B?: A: write every NaN as `nan.0`, whatever its sign bit and standard library, and change nothing in the reader (NaN and infinities still do not read back). B: write NaN as `1.5NaN` and infinities as `1.0Inf` and `-1.0Inf`, as SWI-Prolog does, and have the lexer read these as floats.

Decision (2026-11-06): B. Every NaN is written `1.5NaN`; its sign and payload are not written, and `==` and compare/3 already see every NaN as the same term. The lexer reads `<digits>.<digits>Inf` as plus infinity (any digits, as in SWI) and `<digits>.<digits>NaN` as NaN when the digits make a value from 1.0 up to but not including 2.0 (the `1.xxx` that SWI writes), judged on the digits before the point (`1`, after any leading zeros) and not on the rounded double, so `1.99999999999999999NaN` is a NaN and `0.99999999999999999NaN` is not; a leading `-` makes the number negative as for any float. A name character right after `Inf` or `NaN` (`1.0Info`, `1.0Inf_`), an exponent (`1.0e5Inf`), lower case (`1.0inf`) and other NaN mantissas (`2.0NaN`) are split as before into a float and a variable, a syntax error. In ISO text a variable never directly follows a number, so no conforming text changes meaning. These literals are not overflow, so they read when `float_overflow` is `error`; `1.0e400` stays `syntax_error(float_overflow)` (M2). read/1, consult, number_codes/2, number_chars/2 and atom_number/2 all use the lexer. writeq of `-(X)` for a NaN with its sign bit set gives `- (1.5NaN)`, which reads back as the compound. `mosatest --roundtrip` sees two NaNs as the same term (`src/write/term_equal.cpp`). Tests: `l22_*` in `tests/pl/write.pl`, `tests/pl/read.pl` and `tests/pl/atoms.pl`, and the `float_inf`, `float_nan` and `float_special_args` cases of `tests/pl/syntax.pl`.

Since review M29, the `~e` and `~f` directives of format/2 (`'$format_float'/4`), and since phase 11 also `~E`, `~g` and `~G`, write an infinity or a NaN with this same text, whatever the number of digits and the standard library, and add no padding zeros; the capital-letter directives do not upper-case the `Inf` and `NaN` text (`m29_*`, `fmt_g_inf` and `fmt_G_inf` in `tests/pl/lib_format.pl`).

## 6.4.2 Symbol-character token before a full stop

Before the decision below, the lexer ended a symbol-character token at a `.` that a layout character or `%` follows, unless another `.` came before it (`src/read/lexer.cpp` `Lexer::ends_clause`). So `X = +.` followed by a newline read as `X = +`, and `writeq(t('+.' - b))` wrote `t(+. -b)`, which did not read back (review L17). 6.4.2 forbids only the single character `.` followed by layout as a graphic token, so `+.` there is one token.

A or B?: A: the lexer reads `+.` as one token (6.4.2); `X = +.` then waits for the end. B: keep the lexer and have writeq quote a symbol-character atom that ends in `.`.

Decision (2026-11-03): A (review L17). The full-width `。` follows the same rule, since the decision of 2026-10-31 reads `。` as `.`; quoting atoms that end in `。` (review M8) stays correct. No file in `lib/`, `tests/pl` or `tests/conformance` ends a clause with a symbol character right before `.`.

Implemented (review L17): only the first character of a graphic token can be the end token. `+.`, `#.` and `+。` (in full-width mode) before layout are one name each, and `X = +.` followed by a newline waits for the end. Mosa's own writers that put a full stop after a written term (the REPL's goal and answers, the test runners, mosac's clause comments) put a space before it when the text ends in a graphic character, so `X = '#'.` answers `X = # .`. Where Mosa reads such a goal back (the REPL, the test runners, and the catalog goal and catalog that render messages), it reads it without char_conversion: the flag (7.11) converts the Prolog text being read, the user's text was converted when it was read, and the writer writes the term as it is, so converting it again would change the goal (`X = a.` under `a` to `b` and `b` to `c` answered `X = c`). With `japanese_punctuation` false, `、` and `。` are graphic characters too, so `X = 、.` is incomplete. A byte that is not UTF-8 right before a `.` becomes a U+FFFD (a graphic character, Section 4) that takes the `.`, so the syntax error skips to the next full stop (`l17_bad_byte_takes_dot` in `tests/pl/chars.pl`). This narrows two earlier texts in Section 4 without changing their dated wording: the decision of 2026-10-28 ("`。` is a clause terminator only when a layout character or the end of input follows") now applies to a `。` that starts a token, and of the atoms the M8 interpretation quotes for ending in `。`, only the atom `。` itself still needs the quotes; the others stay quoted, which reads back as well. Since review L23, `initialization(G, main)` keeps a copy of its goal term outside the heap (`export_term`) and neither writes nor reads it, so a later op/3, flag or char_conversion does not change it.

## 9.3.8 and 9.3.9 max/2 and min/2 of an integer and a float

Cor.2 leaves the type of the value implementation dependent when the arguments are an integer and a float (its examples: `max(2.0, 3)` is `3` or `3.0`, `max(0, 0.0)` is `0` or `0.0`; `max(2, 3.0)` is `3.0`). Mosa converts the integer to a float, compares as `<`/2 does and returns a float, so `max(9007199254740993, 9007199254740992.0)` is `9.007199254740992e+15`, smaller than the integer argument (review L1). Returning the chosen argument with its own type would give the exact value, but the type would then depend on the values, and since 9.1.6 (decision A) makes `floor` and the other rounding functions reject integers, `floor(max(0.0, 3))` would raise `type_error(float, 3)` (L1 adversarial review A1).

A or B?: A: compare exactly and return the chosen argument with its own type (`max(2.0, 3) = 3`). B: keep converting to a float and returning a float (`max(2.0, 3) = 3.0`).

Decision (2026-11-05): B. Nothing changes. A comparison with a NaN is false both ways, so `max` gives the left argument and `min` the right one (`max(1, nan) = 1.0`, `max(nan, 1) = nan`); equal values do the same (`max(0, 0.0) = 0.0`). Tests: `l1_max_*` and `l1_min_*` in `tests/pl/arith.pl`.

Once `-0.0` and `0.0` became distinct terms (7.2.2 below), the rule for equal values made `max(-0.0, 0.0)` give `-0.0` and `max(0.0, -0.0)` give `0.0` (adversarial review of L2, A3). A or B?: A: take `-0.0` as smaller than `0.0` and `0`, so `max` gives `0.0` and `min` gives `-0.0` in either order, as the standard order and IEEE 754-2019 maximum and minimum do. B: keep the rule for equal values.

Decision (2026-11-05): A. NaN and other equal values keep decision B. Tests: `l2_max_zero_*` and `l2_min_zero_*` in `tests/pl/arith.pl`.

Since phase 10 (2026-11-12, ADR 0025), an integer beyond the double range cannot be converted, and `max/2` and `min/2` of it and a float raise `evaluation_error(float_overflow)` (8.7, 9.3.8 and 9.3.9 below, default A until the owner confirms).

## 6.3.3 `[]` and `{}` as the name of a compound term

6.3.3 writes a compound term in functional notation as a name token followed by `(`, and `[]` and `{}` are not name tokens (6.4.2; they are atoms formed by 6.3.1.3), so `'[]'(x)` and `'{}'(a)` have no unquoted form in ISO syntax. Mosa writes `writeq('[]'(x))` as `[](x)` and `write_canonical({a})` as `{}(a)`, and its reader reads both back (review L7). The Logtalk portability tests in the conformance suite expect the same: `write_canonical({a})` writes `{}(a)` (`lgt_curly_bracketed_term_07`, `08`), and `[](2,3)` and `{}(2,3)` read as compound terms (`lgt_compound_01`, `02`).

A or B?: A: follow 6.3.3 and quote the name when quoted is true (`'[]'(x)`, `'{}'(a)`); the two Logtalk tests would fail. B: keep `[](x)` and `{}(a)`.

Decision (2026-11-05): B. Nothing changes; `canonical_brace` in `tests/pl/write.pl` pins `{}(a)`.

## 8.2.2 unify_with_occurs_check/2 of terms that are already cyclic

7.3.2 fails a unification with occurs check when it would bind a variable to a term that contains it; 8.2.2 does not say what happens when an argument is already cyclic (7.3.3 leaves such terms undefined). Mosa unified and then failed when the result was cyclic, so `X = f(X), Y = f(Y), unify_with_occurs_check(X, Y)` failed although it binds nothing (review L3).

A or B?: A: fail only when a binding makes a new cycle, as the 7.3.2 check does (`X = f(X), unify_with_occurs_check(A, X)` succeeds, since A does not occur in X). B: also fail when a variable would be bound to a cyclic term.

Decision (2026-11-05): B. A was chosen first, but the adversarial review of L3 found that it makes the answer depend on the order of the bindings: after `X = f(X)`, `unify_with_occurs_check(g(A, A), g(X, f(A)))` succeeded and `unify_with_occurs_check(g(A, A), g(f(A), X))` failed, although the two solve the same equations; whether A stands for an infinite term does not depend on the order. Mosa unifies as `=` does, then walks the values of the variables this call bound, sharing what it has already walked, and fails, undoing the bindings, when one of them is cyclic (`occurs_unify` in `src/builtins/types.cpp`). A unification that fails is as cheap as with `=`, and a call that binds nothing succeeds without a walk. Tests: `l3_occurs_*` in `tests/pl/unify.pl` (`l3_occurs_cyclic_equal`, `l3_occurs_cyclic_unfolded`, `l3_occurs_bind_beside_old` and `l3_occurs_bind_inside_old` fail on 5dd9ed9; `l3_occurs_order_left` succeeded under A), `occurs_check_cyclic` in `tests/stack/deep_builtins.pl` (unchanged).

## 8.2.4 subsumes_term/2 and cyclic terms

Cor.2 defines `subsumes_term/2` with `unify_with_occurs_check/2`. In Mosa that predicate follows the decision B of 8.2.2 and also fails when a variable would be bound to a term that is already cyclic. Read literally, `X = f(X), subsumes_term(f(_), f(X))` would therefore be false, while SWI-Prolog and the bundled suite (`lgt_subsumes_term_2_15`, which expects true) say true. For inputs that are not cyclic, the default unification followed by a comparison of the variables of the specific term gives the same truth value.

A or B?: A: inherit the decision B of 8.2.2 and make such calls false. B: use the default unification and make them true (the same as SWI-Prolog and the suite).

Decision (2026-11-11): owner chose B. The standard leaves terms that are already cyclic undefined (7.3.3), so A would follow only Mosa's own decision for 8.2.2, not the standard. `run_subsumes_term` in `src/builtins/subsumes.cpp` uses the default unification; tests `subsumes_cyclic_*` in `tests/pl/unify.pl`.

## 8 is_list/1 (non-ISO) of a list with cyclic elements

`is_list/1` checked that the whole term was acyclic, so `X = [X], is_list(X)` and `X = f(X), is_list([X])` failed, and `format/2` and `msort/2`, which check their list arguments with it, raised errors for such lists (review L3).

A or B?: A: look at the spine only; a list whose spine ends in `[]` is a list whatever its elements are. B: keep failing when any part of the term is cyclic.

Decision (2026-11-05): A. A cyclic spine (`L = [a|L]`) still fails. Tests: `list_cycle` and `l3_list_*` in `tests/pl/types.pl`, `cyclic list` in `tests/cpp/machine_test.cpp`.

## 7.2.2 Negative zero as a term

7.2.2 orders two floats by value and does not say whether `-0.0` and `0.0` are the same term. Mosa unified them and took them as identical, so `-0.0 == 0.0` held and `sort([0.0, -0.0], L)` gave `[0.0]`, although writeq and copysign tell them apart (review L2).

A or B?: A: make them distinct terms: `-0.0 \== 0.0`, they do not unify, `-0.0 @< 0.0`, and `-0.0 =:= 0.0` stays true. B: keep them the same term.

Decision (2026-11-05): A, as in SWI-Prolog. `same_float_term` in `include/mosa/unify.hpp` is the rule for unification, for `mosatest --roundtrip` and for the matcher of the Logtalk suite; compare/3 and `==` put `-0.0` first; the first-argument index and the generated code of mosac give the two different keys. A NaN still unifies with nothing, not even itself, while `==` and compare/3 take two NaNs as equal (unchanged). Tests: `signed_zero_eq` and `l2_zero_*` in `tests/pl/order.pl`, `m3_keys_zero` and `m3_keys_zero_negative` in `tests/pl/database.pl`, "signed zero" in `tests/cpp/unify_test.cpp`; the decision changed the expectations of these four.

## 9.1.4 sign/1 of a NaN, and integer/1 (non-ISO)

ISO floats have no NaN, so 9.1.4 says nothing about `sign(nan)`; Mosa gave `1.0`. `integer/1` is an SWI-Prolog extension, not ISO; Mosa truncated (`integer(2.5)` was `2`), while SWI rounds to the nearest integer (review L2).

A or B?: for sign/1, A: return the NaN; B: keep `1.0`. For integer/1 (truncation was offered first and not chosen), A: round to the nearest integer with halves away from zero, as SWI does (`integer(-2.5) = -3`); B: round as Mosa's round/1 does, by the floor(x + 1/2) of 9.1.6.1 (`integer(-2.5) = -2`). The first form of the integer/1 question wrongly said that A was the same as round/1; the owner answered again after the correction.

Decision (2026-11-05): A for both. Tests: `l2_sign_*` and `l2_integer_*` in `tests/pl/arith.pl`; `l2_round_neg_half` pins that round/1 does not change.

## 7.4.2 Consulting a path again

ISO prepares a Prolog text once (7.4) and defines no reconsult; SPEC does not define it either. Consulting a path again replaced only the predicates the new contents define, so a predicate that the earlier contents defined and the new ones no longer have stayed, including one declared there with `:- dynamic` (review L11).

A or B?: A: on a reconsult, remove the predicates that the previous load of that path defined and the new contents lack, as SWI-Prolog does; a failed load keeps them. B: keep them.

Decision (2026-11-05): A, under ADR 0020. A predicate made by a goal (assertz) and a multifile predicate stay, and so does one that another file defined later. One that the new contents only declare dynamic, or that a goal of the reload asserted into or retracted from, stays with only the clauses added during the reload. A stale predicate of a nested consult goes when the outermost load succeeds.

## 7.10.2 and 8.12: Line ends of the standard streams on Windows

The decision in "8.12 Newlines in text" makes a newline a single U+000A in text files and memory streams on every OS, and does not leave it to C++ text mode. Files are opened in binary mode, but stdin, stdout and stderr stay in the C runtime's text mode on Windows (review L8): a 0x1A byte in a pipe ends the input, a CRLF from a pipe arrives as LF, and each `nl` on stdout is written as CRLF. A Windows console is read with `ReadConsoleW`, which drops the CR of the CRLF that Enter gives (`console_line_text`, src/platform/console.cpp).

A or B? (review L8): with stdin, stdout and stderr in binary mode on Windows, so that 0x1A is an ordinary byte and a newline is written as LF alone, as on macOS and Linux: A: a pipe's CRLF is read as it is, as from a file and as on macOS and Linux, so get_char/1 sees the CR (read/1 takes it as layout); B: the CR of a pipe's CRLF is dropped on Windows, as the console does and as text mode does today.

Decision (2026-11-05): A. On Windows, `prepare_console` (src/platform/console.cpp) puts stdin, stdout and stderr in binary mode before anything is read or written, in mosa, mosac, mosatest (and `mosatest --diff`) and the executables mosac builds, which call it before they reserve the heap. A program that embeds libmosa keeps its own modes. A 0x1A byte is read like any other, a CRLF from a pipe or a file given as stdin reaches get_char/1 as CR and LF, as on macOS and Linux, and `nl` writes LF alone. The console still drops the CR of the CRLF that Enter gives. Test: ctest `mosa_std_binary`.

## 13211-3 Grammar rules: the source text

13211-1 has no grammar rules; SPEC section 3 takes ISO/IEC 13211-3 as the reference from phase 8. The repository holds no text of 13211-3, and ADR 0024 was written without it, from the behavior that Prolog systems share. So no clause number of 13211-3 is cited yet: the tests of `tests/pl/dcg.pl` are our own until the text is at hand, and its Examples are copied in then. A point where ADR 0024 and the text disagree gets its own entry here with the clause number.

## 7.11.1.2 and 7.11.1.3 `max_integer` and `min_integer` when `bounded` is false

The repository holds no text of ISO/IEC 13211-1, so this is not settled by its wording. Phase 10 makes the flag `bounded` false (7.11.1.1), and whether 7.11.1.2 and 7.11.1.3 define `max_integer` and `min_integer` when `bounded` is false cannot be settled without the text. The conformance suite reads them only when `bounded` is true (`iso_current_prolog_flag_2_03`, `04`), and `tests/pl/read.pl` (the helper `m1_min`, 22 cases) and `flags.pl` read them as the int64 bounds. The phase 9 design said "dropped by 7.11.1.2", which was not checked against the text and is not relied on.

A or B?: A: keep the flags and return the int64 bounds (2^63-1 and -2^63), as SWI-Prolog and Scryer Prolog do. B: remove the flags, so `current_prolog_flag(max_integer, X)` raises `domain_error(prolog_flag, max_integer)` as for any unknown flag (8.17.2; `src/builtins/flags.cpp`).

Until the owner confirms: A.

## 8.7, 9.3.8 and 9.3.9 an integer beyond the double range against a float

The repository holds no text of ISO/IEC 13211-1. Phase 10 allows an integer too large for a double. Comparing it with a float, and `min/2` and `max/2` (decision B of 9.3.8 above), convert the integer to a float. The suite case `lgt_unbounded_float_03` (`float(7^7^7)`) expects `float_overflow`, and its 4 cases that mix floats (`+`, `-`, `*`, `/` with `13.42e+33`) stay inside the double range.

A or B?: A: keep the rule of 9.3.8 (convert to a float), and raise `evaluation_error(float_overflow)` for a value that cannot be converted. B: compare exactly, so `2^2000 > 1.0` is true (this needs an exact comparison in `cmp_nums` and in `min/2` and `max/2`).

Until the owner confirms: A.

## 8.17.4 halt/1

ISO/IEC 13211-1 8.17.4 takes an integer exit status but does not say what an integer the operating system cannot take does. The status reaches the OS as a C++ `int`, and before phase 10 Mosa cast the int64 to `int` without a check, so `halt(4294967296)` exited with status 0. Phase 10 adds integers beyond int64 (Bigs).

A or B?: A: an integer outside C++ `int` gives `representation_error(max_integer)` when positive and `representation_error(min_integer)` when negative (the Flags of 7.12.2 g), the same for an int64 outside `int` and for a Big. B: pass the low bits of the integer to the OS.

Decision (2026-11-13): A (owner decision A). B is rejected. One check, `halt_code_problem` in `src/builtins/control.cpp`, serves two places. The first is the IR arm `arm_halt` in `src/machine/catch.cpp`. Interpreted and translated code both reach it, because the interpreter's `compile.cpp` and mosac's codegen (`emit_body.cpp`) both lower `halt` to `IrTag::Halt`. The second is the built-in entry `run_halt` in `control.cpp`; the tests are `halt_*` in `tests/pl/bigint.pl`. An integer inside `int` still goes to the OS unchanged, which keeps its own low bits (for example 8 bits on POSIX).

## Section 5 `length/2`: ISO or not

SPEC section 5 lists `length/2` as "ISO core", while chapter 14 of the manual gives it the clause number "—". The repository holds no text of 13211-1, so this is not settled by its wording. It decides which column the 24 conformance cases of `length_2` count in: the ISO column, or the library column (SPEC section 9, row 11).

A or B?: A: the manual is right. Fix the wording of SPEC section 5 to "not in 13211-1, kept in `system`" (en/ja); the place (`system`) does not change, and `length_2` counts in the library column. B: SPEC is right. Write the clause number in the table of manual chapter 14 (en/ja), and `length_2` counts in the ISO column. The ISO column, 2291/2326 since Task 3 added the four ISO directories, then moves to 2315/2350 (all 24 `length_2` cases pass since Task 4), so the regression gate's yardstick is rebuilt in that commit.

Until the owner confirms: A. The wording of SPEC section 5 is not touched, and `length_2` counts in the library column.

## Section 7 Stage G `setup_call_cleanup/3`: bindings of a cleanup that a cut runs

The suite case `lgt_setup_call_cleanup_3_26` runs `setup_call_cleanup(true, ndet(_), D = true)` and then expects `var(D)`. Logtalk's lgtunit (the pinned commit 4decb64, `tools/lgtunit/lgtunit.lgt`) runs a `true(Assertion)` test as `(Goal, (\+ \+ catch(Assertion, ...) -> true ; throw(assertion_failure(Assertion))))`, so it checks the assertion right after the body, while the choice point of `ndet(_)` is still alive and the cleanup has not run; `var(D)` holds (the cases `lgt_setup_call_cleanup_3_25` and `_26` are a pair that tests deterministic against nondeterministic). Mosa's runner builds the goal of a `true(Assertion)` test (`LgtKind::Assert`) as `(Body, !), (Check)` (`goal_text` in `src/machine/lgt_file.cpp`), which lgtunit does not do. The cut removes the choice point of `ndet(_)` and so runs the cleanup, and under ADR 0016 the bindings a cleanup makes are kept, so `D` is `true` and `var(D)` fails. The departure is the runner's early cut; ADR 0016 only decides what that cut leaves behind. The design chose not to change the runner's goal shape, because that would change the flow of every in-scope `true(Assertion)` case and could move the ISO column.

A or B?: A: keep ADR 0016 as it is; the one case `lgt_setup_call_cleanup_3_26` stays failing for the reason above, as recorded in this note. B: change ADR 0016 so that the bindings of a cleanup run by a cut are undone (this needs a new ADR that replaces 0016, and touches the engine's cleanup frame, `trim_stack` and the generated code's cut).

Until the owner confirms: A. ADR 0016 is not changed.

## Section 5 `format/2,3`: reference for the phase 11 directives

13211-1 has no `format/2,3`, so the reference for the directives that phase 11 adds (`~+ ~r ~R ~E ~g ~G ~D ~p ~i ~W ~k ~N`) is the cases of the bundled suite, `format_2` and `format_3`. A point the cases do not decide follows the documentation of SWI-Prolog's `format/2`; where the suite and SWI-Prolog disagree, the suite wins (for example, the default radix of `~r` is 8).

- `~r` and `~R` with a radix outside 2 to 36 raise `domain_error(radix, N)`; the suite accepts any `domain_error(_, _)`.
- The columns of `~t~|` and `~+` are counted from the start of the `format` call as column 0, not from the current column of the output stream. This is a known limit: the stream counts characters, not East Asian width, so its column cannot be used as it is. The columns are counted with East Asian width (W and F as 2), as `~t~|` already does ("Section 4 format/2 fill characters by width").
- The directory `portray_1` is out of scope. `portray/1` is not made a built-in; the decision of 8.14 ("print/1 and print/2" above) stands.

## Section 9 phase 11: library column failures left

Recorded (2026-11-14, phase 11 Task 10). After phase 11, the library column of `docs/conformance.md` passes 561 of 566 cases. These are the five failures left, each with its reason:

- `lgt_format_2_tab_table_left_aligned`, `lgt_format_2_tab_table_right_aligned`, `lgt_format_3_tab_table_left_aligned` and `lgt_format_3_tab_table_right_aligned` (actual `existence_error(procedure, ::/2)`). Each body walks the rows of a table with `forall(list::member(Line, Rows), {format(...)})`. `list::member/2` sends a message to Logtalk's library object `list`, and the runner does not implement message sending ("Conformance suite: Logtalk message sending in test bodies" above), so each case stops before `format` runs. The subject itself works: the same calls with `member/2` in place of `list::member/2`, through `with_output_to/2` for `format/2` and through a stream with the alias `out` for `format/3`, give the text each case expects (checked by hand with `mosa` on 2026-11-14). The `~|` and `~+` table columns are covered by `fmt_plus_chain` and `fmt_plus_newline_table` in `tests/pl/lib_format.pl` and by the other `tab_*` cases of `format_2` and `format_3`, which pass. The four cases stay failing as a limit of the runner, not as a difference in `format`.
- `lgt_setup_call_cleanup_3_26` (actual `fail`). The runner cuts the body before it checks `var(D)`, and under ADR 0016 the cleanup that the cut runs keeps its binding ("Section 7 Stage G `setup_call_cleanup/3`: bindings of a cleanup that a cut runs" above). It stays failing under that question's default A until the owner confirms.
