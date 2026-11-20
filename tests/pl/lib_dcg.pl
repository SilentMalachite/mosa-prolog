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
test(expand_term_pushback_body, (expand_term((a, [p] --> [x]), C), C = (a(S0, S) :- (S0 = [x|T1], S = [p|T2])), T1 == T2), true).
% レビュー H-11。'.' の非終端記号は '.'(S0, S) で、それはリストのセル [S0|S] である。前は '.'/2 の Str を作り、
% [S0|S] と単一化しなかった。頭と本体の非終端記号に S0 と S を足して 1024 を超えると representation_error(max_arity)
% （7.11.2.1）。前は 1026 の項数の頭を作った。答えに 1024 個の引数の項を出さないよう、頭は functor/3 で見る。
test(h11_dot_head, (expand_term(('.' --> [a]), (H :- B)), H = [S0|S], B == (S0 = [a|S])), true).
test(h11_dot_body, (expand_term((h11 --> '.'), (_ :- B)), B = [_|_]), true).
test(h11_arity_limit, (functor(F, f, 1022), expand_term((F --> [a]), (H :- _)), functor(H, f, 1024)), true).
test(h11_arity_head, (functor(F, f, 1023), expand_term((F --> [a]), _)), error(representation_error(max_arity))).
test(h11_arity_body, (functor(G, g, 1023), expand_term((h11 --> G), _)), error(representation_error(max_arity))).
