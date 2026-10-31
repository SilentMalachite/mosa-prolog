English | [日本語](0011-bounded-native-stack.ja.md)

Status: Approved (2026-10-30)

## Context

Writing a term nested about 30000 deep overflows the C++ stack, and loading a 50000-element list literal crashes `mosa` (20000 on Windows, whose default stack is 1 MiB).
The functions that recurse once per nesting level or per list element are the writer's `render` family (`render`, `render_op`, `render_list`, `render_functor`, `count`), `Build::data`, `materialize`, `unify_cells`, and the implicit destructor and copy of `IrTerm`.
Owner decision (2) of 2026-10-30 makes the writer iterative in phase 7; SPEC section 6 forbids C++ recursion in execution (trampoline), and ADR 0010 keeps a `GcInhibit` around portray only until the writer is rewritten.

## Decision

The functions above, and the walkers of the same shape (copy_term `export_rec`, findall `encode`/`copy_cell`, `compare_seen`, `collect_vars`, clause/2 `remember_slot`/`expose`, `acyclic_term`, `term_equal`'s `match`, the answer's `note_shown`), keep their work on an explicit `std::vector` instead of the C++ stack. `IrTerm` gets an iterative destructor, copy constructor and copy assignment (move stays default); its fields do not change (ADR 0003).
The writer appends to its output in order instead of returning strings; spacing (`glue`) compares the last character written with the first character of the next token. The portray `GcInhibit` is removed and the writer's pending work, `open_`, `counts_` and `assigned_` are registered with a `RootScope` (ADR 0010), rebuilding address-keyed tables after a GC.
Targets: terms built at run time up to depth 1,000,000 (`f(f(...))`, lists, left-nested `a+b+...`, right-nested `(a,b,...)`) and list literals in source up to 200,000 elements, both under a 1 MiB stack; output is byte-identical to today except the 7.10.5 fix for operator atoms as operands; `mosatest --bench` is within 5% for the interpreter and the translator.

## Rejected alternatives

Running on a thread with a large stack: it only moves the limit, and an embedding program or a `mosac` executable cannot choose the caller's thread.
A depth limit with an error: ISO puts no depth limit on terms.
Making the reader iterative too: nesting in source (`f(f(...))`, `[[...]]`, `(...)`, chains of xfy right operands) is rare at 10,000 levels, flat lists and yfx left chains already read iteratively, and `Build::goal`'s control constructs stay recursive because the reader hits its limit first.

## Consequences

`docs/iso-notes.md` (English and Japanese), section "7.1 Nesting depth of terms", is rewritten so that the limit covers only nesting in the reader; `mosac_long_list` runs 20000 elements on every OS.
On approval, ADR 0010's sentence on the portray exception (English and Japanese) is amended to say the exception is removed by this ADR.
No code is written on this ADR before it is approved.

## Amendment (2026-10-31, approved 2026-10-31)

Context: a pure explicit stack made the interpreter 30–80% slower (bench, phase7-iterative Task 6), and the reader limit measured under 1 MiB is about 600 levels of `f(f(...))`, not the 10,000 assumed above.
Decision: the hot walkers (`IrTerm` copy and destruction, `materialize`, `unify_cells`, `compare_seen`) recurse natively up to depth 64, passing the depth as an argument, and move only deeper subtrees to an explicit stack, keeping the visiting and allocation order; `IrTerm` move assignment is user-written so that `term = std::move(term.args[1])` stays valid; a control construct's extra is moved out and back during `launch` instead of copied, and is not written back when a synchronous throw discarded it (`extras_low`).
Rejected: a thread-local depth counter (its cost showed in the bench) and a per-step copy of the extra. Consequences: the reader stays recursive; its measured limits are in iso-notes 7.1.

## Amendment 2 (2026-10-31, approved 2026-10-31)

Context: the rejected alternative above kept `Build::goal` recursive because the reader limits nesting in source, but goals built at run time are not read: `call/1` or `assertz/1` of a conjunction 100,000 long and `X is E` with `E` 100,000 deep end in SIGSEGV under an 8 MiB stack (review of main, 2026-10-31). Arithmetic evaluation (`eval_expr`, `eval_args`) was missing from the list above.
Decision: `Build::goal`, `body_callable`, database `goal_form`, the left operand of `IrTag::And` in `launch`, and `eval_expr`/`eval_args` follow the first amendment (native recursion to depth 64, deeper subtrees on an explicit stack), keeping the visiting order so the first error raised does not change (outer functor before arguments, arguments left to right). The IR does not change (ADR 0003). Target: left- and right-nested conjunctions and expressions of depth 1,000,000 under a 1 MiB stack, checked in `tests/stack/deep_builtins.pl`; `mosatest --bench` within 5%.
Rejected: flattening conjunctions into an n-ary IR node (changes ADR 0003); a depth limit with `resource_error` (ISO puts no depth limit on terms). Consequences: the `mosac` code generator (`emit_body`, `check`) stays recursive because its input comes from the reader; iso-notes 7.1 gains one sentence saying goals and expressions built at run time have no depth limit.
