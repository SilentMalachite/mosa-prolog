% フェーズ6 Task 13。竹内関数 tak(17, 11, 6)。整数の算術と比較、切りでの決定的な再帰で、1 回に 16701 回呼ぶ。
% それを失敗駆動で N 回。決まりは nrev.pl と同じ（N は旗 argv から、statistics/2 は使わない）。
:- initialization(main, main).

tak(X, Y, Z, A) :- X =< Y, !, Z = A.
tak(X, Y, Z, A) :-
    X1 is X - 1,
    Y1 is Y - 1,
    Z1 is Z - 1,
    tak(X1, Y, Z, A1),
    tak(Y1, Z, X, A2),
    tak(Z1, X, Y, A3),
    tak(A1, A2, A3, A).

loop(_).
loop(N) :- N > 1, N1 is N - 1, loop(N1).

bench(N) :- loop(N), tak(17, 11, 6, _), fail.
bench(_).

% 正しさ。tak(17, 11, 6) は 7。
check :-
    tak(17, 11, 6, A),
    A == 7.

% check は \+ \+ の中で呼び、使ったヒープを後戻りで返す。GC はフェーズ7なので、決定的な tak 1 回でヒープの半分を
% 使い、そのままでは bench の 1 回目が上限を超える。
main :-
    \+ \+ check,
    current_prolog_flag(argv, [Arg|_]),
    atom_codes(Arg, Codes),
    number_codes(N, Codes),
    bench(N).
