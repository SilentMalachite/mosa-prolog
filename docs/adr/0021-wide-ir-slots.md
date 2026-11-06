English | [日本語](0021-wide-ir-slots.ja.md)

Status: Approved (2026-11-06; the owner chose A for review M31 and asked for it to be implemented, which counts as approval). This changes the IR of [ADR 0003](0003-execution-ir.md).

## Context

`IrTerm::slot` and `Clause::vars` have 16 bits, so a goal or clause that needs more than 65,536 slots wraps the slot number and some of its arguments silently take the value of another slot (review M31). A slot is taken by each unbound variable and, since M28, by each bound compound argument that a run-time goal refers to in place.
M28 kept run-time goals under the limit in stages: past 63,488 slots it compiles the remaining child goals when they run (`call/1` leaves), then it compiles again copying the small arguments, and last it copies everything, which expands sharing as a tree and can still wrap. Clauses have no stages.

## Decision

`IrTerm::slot` and `Clause::vars` become `std::uint32_t`, and so do the slot numbers of the compiler, of the answer templates of findall/3 and bagof/3, and of mosac's code generator. `IrTerm` stays 56 bytes: the wider field takes the padding that followed it.
`compile_goal` refers to each bound compound argument in place and never compiles a goal again: the stage that copies small arguments, the full copy of run-time goals and the cycle marking that only the full copy needed go. The deferral of M28 stays: past 63,488 slots the remaining child goals are compiled when they run, so the rest of a goal that never runs builds no IR. The node budget of M28 stays too and still stops a shared control skeleton at the heap limit.
A goal or clause that would need 2^32 slots stops at the heap limit (`ResourceLimit`), since its environment alone would take 32 GiB.

## Rejected alternatives

B: stop a goal or clause that needs more than 65,536 slots with a resource error. `length(L, 70000), assertz(p(L))` would fail although it fits in memory, and the M28 stages that compile again would stay.
Keep the stages that compile again. They would never run, and their code would only look as if it guards something.
Drop the deferral too. `\+ call((fail, G))` with 240,000 goals in G would build the IR of all of them, about four times the term, and stop at a 32 MiB limit (the Codex adversarial review of M31).
64-bit slots. `IrTerm` would grow by 8 bytes, and no goal that fits under a heap limit of today's machines needs 2^32 slots.

## Consequences

No goal or clause runs with a wrong slot. A run-time goal past 65,536 slots runs as one that stays under it; the goal_shared check that a copy past the slots stops at an 8 MiB limit now runs the goal, since that copy no longer exists. The M28 tests of the deferral rules still test the deferral.
mosac's generated code keeps its variable names, and the fingerprint of generated functions hashes the same values.
Tests: `m31_*` in tests/pl/control.pl, tests/cpp/goal_shared_test.cpp.
