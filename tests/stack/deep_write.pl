% 深さ 1,000,000 の項を write/1、writeq/1、print/1、write_canonical/1 で書く（ADR 0011）。
% small_stack.cmake が 1 MiB のスタックで動かし、各行を「長さ|先頭 12 バイト|末尾 12 バイト」に縮めて比べる。
% with_output_to/2 で受けないのは、ゴールの翻訳（Build::data）が項の深さだけ再帰するため（計画の Task 5）。
% 項は数え上げの繰り返しで作る。mosa の lib には numlist/3 が無い。
:- initialization(main, main).

depth(1000000).

% print/1 の portray が深い項の中でも呼ばれ、書き換えた字面が出ること。
portray(secret) :- write(hidden).

nest(0, X, X) :- !.
nest(N, X, Y) :- M is N - 1, nest(M, f(X, secret), Y).

minus(0, X, X) :- !.
minus(N, X, Y) :- M is N - 1, minus(M, -(X), Y).

sum(K, N, X, X) :- K > N, !.
sum(K, N, X, Y) :- K1 is K + 1, sum(K1, N, X+K, Y).

conj(0, X, X) :- !.
conj(N, X, Y) :- M is N - 1, conj(M, (a, X), Y).

count(0, L, L) :- !.
count(N, L0, L) :- M is N - 1, count(M, [N|L0], L).

main :-
    depth(D),
    nest(D, a, F), write(F), nl, print(F), nl,
    minus(D, a, M), writeq(M), nl,
    sum(2, D, 1, S), write(S), nl,
    conj(D, b, C), writeq(C), nl,
    count(D, [], L), write_canonical(L), nl, print(L), nl.
