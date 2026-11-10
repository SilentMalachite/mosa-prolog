English | [日本語](0024-dcg-translation.ja.md)

Status: Approved (2026-11-10; the owner opened phase 8 for grammar rules and approved this ADR, decision A). SPEC section 3 puts grammar rules in scope and section 9 gives phase 8; this ADR fixes where `-->` is translated and the shape of the clause that results.

## Context

`-->` is an operator (1200, xfx), but the loader refuses a term `H --> B` (`take_term` in `src/machine/load.cpp`), so a file that holds one does not load. `term_expansion/2` is not implemented, so a translation written in Prolog has no hook to run from.
`mosac` reads a text through the same `take_term`, and the IR is the common input of the interpreter and the translator (ADR 0003, 0009).
A program sees the translated clause through `clause/2`, error terms and the place of a cut, so the shape is hard to change once programs depend on it.

## Decision

The loader translates `H --> B` to `H' :- B'` in C++ (`src/machine/dcg.cpp`) before `compile_clause`; the IR, the Cell and `mosac` do not change. `H'` is `H` with two more arguments `S0, S`; a head `H, Pushback` adds `S = Pushback ++ S1` after the body. `M:(H --> B)` stays refused. `phrase/2,3` are control constructs in `system`, like `call/N`: they check their arguments, translate `G` as a body from `L` to `R` with the same code (`R` is `[]` for phrase/2), and run it as call/1 does, so a cut in it is local (revision 1: the first text said "call `G` with `L` and `R` added", which fails for `phrase([a], L)`).
Body: a non-terminal gets `S0, S`; a list `[a,b]` is the goal `S0 = [a,b|S]` and `[]` is `S0 = S`, never folded into the head; `,`, `;`, `|` and `->` keep their structure; `M:B` is `M:B'`; `\+ A` is `\+ A', S0 = S`; `{G}` is `call(G), S0 = S`; `!` is `!, S0 = S`; `call(G, A…)` is `call(G, A…, S0, S)`; a variable `V`, and a control construct met again inside itself (a cyclic body given to phrase/2,3), is `phrase(V, S0, S)`, as call/1 translates a cyclic goal when it reaches the back edge. The body is walked with an explicit stack, not recursion (revision 2, Codex review P1: a deep or cyclic body crashed the C++ stack).
Errors at load, by the `formal_*` helpers: a variable head is `instantiation_error`; a head or body part that is not callable is `type_error(callable, Culprit)`; a pushback or terminal list with a variable tail is `instantiation_error`, and one that is not a list is `type_error(list, Culprit)`. phrase/2,3: a variable `G` is `instantiation_error`, a `G` with a part that is not callable is `type_error(callable, G)` (the whole `G`, as call/1 does), and an `L` or `R` that is neither a list nor a partial list is `type_error(list, Culprit)`.

## Rejected alternatives

A translation in Prolog under `term_expansion/2`: the hook does not exist, and a library in `lib/` is not visible in `user` without `use_module` (SPEC section 3), while a `-->` clause has to load with no import.
Unifying terminals in the head (`a([x|S], S)`): a cut or `{G}` before a terminal would then run after the unification, so `a --> !, [x]` and `a --> {G}, [x]` change meaning.
A new IR instruction for terminals: it changes the IR of phase 2 for a gain that first-argument indexing does not need here, and `mosac` would need the same instruction.

## Consequences

`assertz((a --> b))` adds a clause of `-->/2` as before, and `term_expansion/2`, `goal_expansion/2` and `expand_term/2` stay unimplemented; listing a grammar rule shows the translated clause.
Tests: `tests/pl/dcg.pl` (one test per body form, pushback, the cut, load errors, phrase/2,3 errors by `Formal`), a Japanese grammar in `tests/pl/japanese.pl`, both under `mosatest --diff`.
The limit "grammar rules are not supported" leaves the CHANGELOG's known limits, manual chapter 14 and STATUS when phase 8 ends; the source text of 13211-3 is noted in iso-notes ("13211-3").

## Revision 3 (2026-11-11, approval pending)

Context: a text that exports or declares a non-terminal cannot name it as `Name//Arity` (`type_error(predicate_indicator, greet//0)`), a grammar body cannot hold `*->` although the control construct exists, and there is no `expand_term/2` to translate one rule at run time. These were the known limits of release 0.1.2.
Decision: (1) `Name//Arity` in export lists, import lists (`use_module/2`) and the directives `dynamic/1`, `discontiguous/1` and `multifile/1` means `Name/(Arity+2)`; the errors follow `/` (`instantiation_error`, `type_error(atom, N)`, `type_error(integer, A)`, `domain_error(not_less_than_zero, A)`, `representation_error(max_arity)` when `A+2 > 1024`). `abolish/1`, `current_predicate/1` and `retract/1` keep only `/` (8.9.4.3), so `abolish(foo//1)` stays `type_error(predicate_indicator, foo//1)`. The code that reads them is `read_indicator` in `src/module/qualify.cpp` (export, import) and `one_indicator` in `src/machine/load_directive.cpp` (the three directives), not `parse_indicator` in `src/builtins/database.cpp`, which is `abolish/1`'s. (2) `(C *-> T)` in a grammar body becomes `(C' *-> T')` with `C` from `S0` to `S1` and `T` from `S1` to `S`, like `->`; inside `;` it gives `(C' *-> T' ; E')`. (3) `expand_term/2` lives in `library(dcg)` (`lib/dcg.pl`, `:- module(dcg, [expand_term/2]).`), not in `system`: SPEC section 3 keeps non-ISO predicates in `library(*)`. It calls the hidden built-in `'$dcg_translate'/2`, the pattern of `library(format)` and `'$format_float'/4`, which runs the loader's translation (`dcg_rule` in `src/machine/dcg.cpp`) at run time; a term that is not `-->/2` comes back unchanged.
Rejected: `expand_term/2` in `system` (an added exception to SPEC section 3); accepting `//` in `abolish/1` too (8.9.4.3 says `type_error(predicate_indicator)`); translating in Prolog with `term_expansion/2` (the hook does not exist, as in the original decision).
Consequences: `term_expansion/2` and `goal_expansion/2` stay out and `assertz/1` still does not translate; the three limits leave the CHANGELOG's known limits when the code lands; tests in `tests/pl/modules.pl`, `tests/pl/dcg.pl`, `tests/pl/lib_dcg.pl` and `tests/cpp/dcg_test.cpp`.
