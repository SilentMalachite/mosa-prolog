English | [日本語](README.ja.md)

ADRs are numbered sequentially. Each ADR gives "Context, Decision, Rejected alternatives, Consequences" in 3 lines or fewer each, and is implemented only after approval.

| No. | Title | Status |
| --- | --- | --- |
| [0001](0001-cell-representation.md) | Cell representation (64-bit tagged integer) | Approved (2026-10-24) |
| [0002](0002-cell-layout.md) | Tag numbers, immediate integer range, functor word bit layout | Approved |
| [0003](0003-execution-ir.md) | Execution IR and trampoline | Approved (2026-10-24) |
| [0004](0004-clause-database.md) | Location of dynamic clauses and choice point indexes | Approved (2026-10-25) |
| [0005](0005-streams.md) | Stream representation, defaults, aliases | Approved (2026-10-25) |
| [0006](0006-modules.md) | Module key, context module in the goal frame, operator scope | Approved (2026-10-28) |
| [0007](0007-messages.md) | Message catalogs, flag message_language, syntax_error Key | Approved (2026-10-28) |
| [0008](0008-logical-update-view.md) | Logical update view: dead clauses kept for open scans | Approved (2026-10-28) |
| [0009](0009-native-code.md) | Generated code: coexistence and calling convention | Approved (2026-10-29) |
| [0010](0010-gc-heap-growth.md) | GC and heap growth: reserved address space, safe-point sliding collector | Approved (2026-10-30) |
| [0011](0011-bounded-native-stack.md) | Bounded native stack: iterative writer, loader, unification and term walkers | Approved (2026-10-30) |
| [0012](0012-deterministic-exit.md) | Deterministic exit pops the catch/3 and *-> entries | Approved (2026-10-31) |
| [0013](0013-inline-collection.md) | Collections run inside the caller's loop, without native recursion | Approved (2026-10-31) |
| [0014](0014-catch-goal-as-data.md) | A catch/3 goal with a variable in goal position runs as call/1 | Approved (2026-11-01) |
| [0015](0015-first-argument-index.md) | Stable clause ids, generations and a first-argument index | Approved (2026-11-02) |
| [0016](0016-call-cleanup.md) | setup_call_cleanup/3: a cleanup entry and cleanups queued to the next step | Approved (2026-11-03) |
| [0017](0017-failed-load-keeps-asserted.md) | A failed load keeps the predicates its goals made with assert | Approved (2026-11-04) |
| [0018](0018-predicate-lookup.md) | Predicate lookup by key: an open-addressing index on the predicate table and the load's snapshots | Approved (2026-11-04) |
| [0019](0019-answer-aliases.md) | Toplevel answers show the aliases of one unbound variable as a chain | Approved (2026-11-04) |
| [0020](0020-reconsult-ownership.md) | A reconsult removes the predicates the file no longer defines, applied by the outermost load | Approved (2026-11-05) |
| [0021](0021-wide-ir-slots.md) | 32-bit IR slots; a run-time goal is never compiled again with copies | Approved (2026-11-06) |
| [0022](0022-shared-asserted-terms.md) | An asserted clause builds each repeated deep subterm once per call | Approved (2026-11-07) |
| [0023](0023-mosac-dist.md) | `mosac dist`: the archive is written by mosac itself, with the runtime's LICENSE, NOTICE and a README | Approved (2026-11-10) |
| [0024](0024-dcg-translation.md) | Grammar rules: the loader translates `-->` in C++; terminals are body unifications; `phrase/2,3` in `system` | Proposed (2026-11-10) |
