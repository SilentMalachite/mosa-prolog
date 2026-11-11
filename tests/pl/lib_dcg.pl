:- use_module(library(dcg)).
% expand_term/2 は文法規則 1 つを訳す（ADR 0024 改訂 3）。規則でない項はそのまま返し、未束縛は instantiation_error。
:- dynamic(exp_a//0).
:- dynamic(exp_b//0).
test(expand_term_rule, (expand_term((a --> [x], b), C), C = (H :- _), H = a(_, _)), true).
test(expand_term_assert, (expand_term((exp_a --> [x], exp_b), C), assertz(C), assertz((exp_b([y|S], S))), phrase(exp_a, [x, y])), true).
test(expand_term_other, expand_term(foo(1), X), solutions([X = foo(1)])).
test(expand_term_var, expand_term(_, _), error(instantiation_error)).
test(expand_term_var_head, expand_term((_ --> [x]), _), error(instantiation_error)).
test(expand_term_bad_head, expand_term((1 --> [x]), _), error(type_error(callable, 1))).
test(expand_term_bad_list, expand_term((a --> [x|_]), _), error(instantiation_error)).
test(expand_term_pushback, (expand_term((a, [p] --> [x]), C), C = (a(S0, S) :- _), var(S0), var(S)), true).
