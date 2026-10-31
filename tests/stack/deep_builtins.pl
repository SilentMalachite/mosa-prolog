% 項を歩く組込と、実行時に組み立てたゴールと式を、深さ 1,000,000 の項で 1 つずつ確かめる（ADR 0011 と改訂 2、
% 1 MiB のスタックで動かす。acyclic_term は unify_with_occurs_check/2 と is_list/1 を通して見る。
% 各行は「組込: 結果」。循環項の行（_cyclic）は、書き直す前と同じ結果であること。
% 項は数え上げの繰り返しで作る。mosa の lib には numlist/3 が無い。
:- initialization(main, main).

depth(1000000).

nest(0, X, X) :- !.
nest(N, X, Y) :- M is N - 1, nest(M, f(X), Y).

count(0, L, L) :- !.
count(N, L0, L) :- M is N - 1, count(M, [N|L0], L).

% lists ライブラリを読まずに済むよう、member/2 と同じものをここに置く。
mem(X, [X|_]).
mem(X, [_|T]) :- mem(X, T).

% 右に入れ子の連言 (true,(true,...)) と、左に入れ子の連言 ((...,true),true)。
conj_right(0, true) :- !.
conj_right(N, (true, G)) :- M is N - 1, conj_right(M, G).

conj_left(0, true) :- !.
conj_left(N, (G, true)) :- M is N - 1, conj_left(M, G).

% 左に入れ子の式 X+1+1+... と、右に入れ子の式 1+(1+(...+X))。
sum_left(0, X, X) :- !.
sum_left(N, X, Y) :- M is N - 1, sum_left(M, X+1, Y).

sum_right(0, X, X) :- !.
sum_right(N, X, Y) :- M is N - 1, sum_right(M, 1+X, Y).

fdepth(T, N) :- fdepth(T, 0, N).
fdepth(T, N0, N) :- nonvar(T), T = f(X), !, N1 is N0 + 1, fdepth(X, N1, N).
fdepth(_, N, N).

% 各行は \+ \+ で束縛を戻し、前の行の深い項を次の行へ持ち越さない。
show(Name, Goal) :-
    (   \+ \+ catch(Goal, E, (write(Name: E), nl, fail))
    ->  write(Name: ok), nl
    ;   write(Name: failed), nl
    ).

deep(F) :- depth(D), nest(D, a, F).

main :-
    show(copy_term_cyclic, (X1 = f(X1), copy_term(X1, Y1), Y1 = f(Z1), Z1 == Y1)),
    show(findall_cyclic, (X2 = f(X2), findall(X2, true, [Y2]), Y2 = f(Z2), Z2 == Y2)),
    show(bagof_cyclic, (X3 = f(X3), bagof(X3, true, [Y3]), Y3 = f(Z3), Z3 == Y3)),
    show(compare_cyclic, (X4 = f(X4), Y4 = f(Y4), compare(O4, X4, Y4), O4 == (=))),
    show(eq_cyclic, (X5 = f(X5), Y5 = f(Y5), X5 == Y5)),
    show(term_variables_cyclic, (X6 = f(X6, V6), term_variables(X6, Vs6), Vs6 == [V6])),
    show(clause_cyclic, (X7 = f(X7), assertz(cyc(X7)), clause(cyc(Z7), true), Z7 = f(A7), var(A7))),
    show(occurs_check_cyclic, (X8 = f(X8), \+ unify_with_occurs_check(_, X8))),
    show(is_list_cyclic, (X9 = [a|X9], \+ is_list(X9))),
    depth(D),
    show(copy_term, (deep(F), nest(D, V, FV), copy_term(F-FV, C-CV), fdepth(C, D), fdepth(CV, D),
                     nest(D, W, CV), var(W), W \== V)),
    show(findall, (deep(F), findall(F, true, [G]), fdepth(G, D))),
    show(bagof, (deep(F), bagof(K, mem(K, [F]), [B]), fdepth(B, D))),
    show(bagof_witness, (deep(F), bagof(K, mem(K-W, [x-F, y-F]), Bs), Bs == [x, y], fdepth(W, D))),
    show(compare, (deep(F1), deep(F2), depth(D), nest(D, b, F3), compare(O1, F1, F2), O1 == (=),
                   compare(O2, F1, F3), O2 == (<))),
    show(eq, (deep(F1), deep(F2), nest(D, b, F3), F1 == F2, F1 \== F3)),
    show(term_variables, (nest(D, V, FV), term_variables(FV, [U]), U == V, length(L, D),
                          term_variables(L, Vs), length(Vs, D))),
    show(clause, (deep(F), assertz(deep_fact(F)), clause(deep_fact(X), true), fdepth(X, D),
                  count(D, [], L), assertz(long_fact(L)), clause(long_fact(Y), true), length(Y, D))),
    show(occurs_check, (deep(F), unify_with_occurs_check(Y, F), Y == F)),
    show(is_list, (count(D, [], L), is_list(L))),
    show(call_conj_right, (conj_right(20000, G), call(G))),
    show(call_conj_left, (conj_left(D, G), call(G))),
    show(assertz_conj_left, (conj_left(D, G), assertz((deep_left :- G)), deep_left)),
    show(call_not_callable, (conj_right(D, G), catch(call((G, 1)), error(type_error(callable, _), _), true))),
    show(assertz_not_callable, (conj_right(D, G),
                                catch(assertz((bad :- G, 1)), error(type_error(callable, _), _), true))),
    show(is_left, (sum_left(D, 0, E), X is E, X =:= D)),
    show(is_right, (sum_right(D, 0, E), X is E, X =:= D)),
    show(compare_arith, (sum_left(D, 0, E), sum_right(D, 0, F), E =:= F)),
    show(is_instantiation, (sum_left(D, _, E), catch(_ is E, error(instantiation_error, _), true))),
    show(is_evaluable, (sum_right(D, foo, E), catch(_ is E, error(type_error(evaluable, foo/0), _), true))).
