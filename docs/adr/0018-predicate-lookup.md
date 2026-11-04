English | [日本語](0018-predicate-lookup.ja.md)

Status: Approved (2026-11-04; the owner asked for review M25 to be fixed before M22, which counts as approval)

## Context

`find_pred` walked the whole predicate table, and every call of a user predicate (`enter_user` → `resolve_pred`), every retry (`redo_clause`), assert, retract and clause/2 looks a predicate up by (module, name, arity): with 40,000 predicates a call cost about 28 µs (review M25).
A load scanned the table again for every clause (`add_clause`) and its snapshots for every predicate it touched first (`remember_pred`), so a consult of P one-clause predicates took O(P²) (1.5 s for 40,000).
The table is kept in serial order (M18, `pred_by_serial`), and abolish and a failed load erase from the middle, so positions shift.

## Decision

`Impl::predicates` is a `PredicateTable`: the same vector in serial order plus a `KeyIndex`, an open-addressing index (linear probing, a power-of-two number of 32-bit slots, at least two per predicate) that holds positions and compares the key with the predicate it points at. Only the table adds and erases: an erase empties its slot by backward shift and lowers the slots of the predicates behind it, found from their keys, and the failed load's `erase_if` rebuilds the index. A predicate's key does not change once it is in the table.
A load's snapshots are a `LoadSnapshots`: the list, a `KeyIndex` that points at each key's last snapshot, and a link from each snapshot to the one before it with the same key. `remember_pred` walks only that chain, newest first, as before.
`add_clause` and `erase_predicate` use the index instead of their own scans. Calls do not cache the predicate they resolved.

## Rejected alternatives

A cache in the call (goal or IR) of the predicate it resolved: abolish, a failed load and a later import can drop or change the answer, so every cache would need an invalidation; the index alone removes the factor P.
`std::unordered_map` from the key to the position (tried first): a node per key, about 60 bytes with libc++ (+4 MB for 40,000 one-clause predicates), and a modulo and a node walk on every lookup.
An index from the key to the serial, read through `pred_by_serial`: an erase would not renumber, but every call would pay a binary search over the table.

## Consequences

A lookup is O(1) on average, hit or miss. With 40,000 predicates, 100,000 calls take 53 ms instead of 5,544 ms; the consult of 40,000 one-clause predicates takes 0.04 s instead of 1.41 s, and 40,000 assertz of new predicates 0.11 s instead of 0.64 s. The index costs 8 to 16 bytes a predicate (maximum RSS +0.3 and +0.5 MB for those two loads). An erase fixes only the slots of the predicates behind it, like the vector's own shift, so abolishing 50,000 predicates from the back takes 85 ms instead of 1,934 ms ([bench.md](../bench.md#cost-of-the-predicate-index)). Each load still walks the table once, and `resolve_pred` still searches modules and imports linearly.
In default builds nrev is 7.1% slower in the interpreter. Built with `-falign-functions=64` in both trees, it takes 0.4% more cycles and retires 0.55% more instructions: the change moved functions in the binary, and a lookup costs about what the scan of nrev's 7-predicate table did.
Tests: `pred_table_test` (index, erases, chains across growth) and `pred_scale_test`, which fails on 1ee2e8b (it is serial and left out of the GC-stress job, like `index_scale`); `m25_*` in `tests/pl/database.pl`.
