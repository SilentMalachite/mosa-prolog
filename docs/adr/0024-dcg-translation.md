English | [日本語](0024-dcg-translation.ja.md)

Status: Proposed (2026-11-10; the owner opened phase 8 for grammar rules, and the approval of this ADR is asked). SPEC section 3 puts grammar rules in scope and section 9 gives phase 8; this ADR fixes where `-->` is translated and the shape of the clause that results.

## Context

`-->` is an operator (1200, xfx), but the loader refuses a term `H --> B` (`take_term` in `src/machine/load.cpp`), so a file that holds one does not load. `term_expansion/2` is not implemented, so a translation written in Prolog has no hook to run from.
`mosac` reads a text through the same `take_term`, and the IR is the common input of the interpreter and the translator (ADR 0003, 0009).
A program sees the translated clause through `clause/2`, error terms and the place of a cut, so the shape is hard to change once programs depend on it.

## Decision

The loader translates `H --> B` to `H' :- B'` in C++ (`src/machine/load_dcg.cpp`) before `compile_clause`; the IR, the Cell and `mosac` do not change. `H'` is `H` with two more arguments `S0, S`; a head `H, Pushback` adds `S = Pushback ++ S1` after the body. `M:(H --> B)` stays refused. `phrase/2,3` are control constructs in `system`, like `call/N`: they check their arguments and then call `G` with `L` and `R` added (`R` is `[]` for phrase/2).
Body: a non-terminal gets `S0, S`; a list `[a,b]` is the goal `S0 = [a,b|S]` and `[]` is `S0 = S`, never folded into the head; `,`, `;`, `|` and `->` keep their structure; `\+ A` is `\+ A', S0 = S`; `{G}` is `call(G), S0 = S`; `!` is `!, S0 = S`; `call(G, A…)` is `call(G, A…, S0, S)`; a variable `V` is `phrase(V, S0, S)`.
Errors at load, by `throw_error`: a variable head is `instantiation_error`; a head or body that is not callable is `type_error(callable, Culprit)`; a pushback that is not a list is `type_error(list, Culprit)`. phrase/2,3: a variable `G` is `instantiation_error`, a non-callable `G` is `type_error(callable, G)`, and an `L` or `R` that is neither a list nor a partial list is `type_error(list, Culprit)`.

## Rejected alternatives

A translation in Prolog under `term_expansion/2`: the hook does not exist, and a library in `lib/` is not visible in `user` without `use_module` (SPEC section 3), while a `-->` clause has to load with no import.
Unifying terminals in the head (`a([x|S], S)`): a cut or `{G}` before a terminal would then run after the unification, so `a --> !, [x]` and `a --> {G}, [x]` change meaning.
A new IR instruction for terminals: it changes the IR of phase 2 for a gain that first-argument indexing does not need here, and `mosac` would need the same instruction.

## Consequences

`assertz((a --> b))` adds a clause of `-->/2` as before, and `term_expansion/2`, `goal_expansion/2` and `expand_term/2` stay unimplemented; listing a grammar rule shows the translated clause.
Tests: `tests/pl/dcg.pl` (one test per body form, pushback, the cut, load errors, phrase/2,3 errors by `Formal`), a Japanese grammar in `tests/pl/japanese.pl`, both under `mosatest --diff`.
The limit "grammar rules are not supported" leaves the CHANGELOG's known limits, manual chapter 14 and STATUS when phase 8 ends; the source text of 13211-3 is noted in iso-notes ("13211-3").
