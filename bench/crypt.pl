% フェーズ6 Task 13。覆面算（crypt）。偶数 E と奇数 O だけが分かっている掛け算
%     OEE
%   *  EE
%   -----
%    EOEE
%    EOE
%   -----
%    OOEE
% を、数字のリスト（下の桁から）の掛け算と足し算で全探索する。それを失敗駆動で N 回。
% 決まりは nrev.pl と同じ（N は旗 argv から、statistics/2 は使わない）。
:- initialization(main, main).

odd(1). odd(3). odd(5). odd(7). odd(9).
even(0). even(2). even(4). even(6). even(8).
lefteven(2). lefteven(4). lefteven(6). lefteven(8).

zero([]).
zero([0|L]) :- zero(L).

% mult(Ds, D, Ps): 下の桁からの数字の列 Ds に 1 桁の D を掛けた列 Ps（繰り上がりの 2 桁を必ず足す）。
mult(Ds, D, Ps) :- mult(Ds, D, 0, Ps).
mult([A|As], D, Carry, [B|Bs]) :-
    X is A * D + Carry,
    B is X mod 10,
    NewCarry is X // 10,
    mult(As, D, NewCarry, Bs).
mult([], _, Carry, [C, Cend]) :-
    C is Carry mod 10,
    Cend is Carry // 10.

% sum(As, Bs, Cs): 下の桁からの数字の列の和。
sum(As, Bs, Cs) :- sum(As, Bs, 0, Cs).
sum([A|As], [B|Bs], Carry, [C|Cs]) :- !,
    X is A + B + Carry,
    C is X mod 10,
    NewCarry is X // 10,
    sum(As, Bs, NewCarry, Cs).
sum([], Bs, 0, Bs) :- !.
sum(As, [], 0, As) :- !.
sum([], [B|Bs], Carry, [C|Cs]) :- !,
    X is B + Carry,
    C is X mod 10,
    NewCarry is X // 10,
    sum([], Bs, NewCarry, Cs).
sum([A|As], [], Carry, [C|Cs]) :- !,
    X is A + Carry,
    C is X mod 10,
    NewCarry is X // 10,
    sum([], As, NewCarry, Cs).
sum([], [], Carry, [Carry]).

% 被乗数は A B C、乗数は D E（どちらも上の桁から）。答えは被乗数、乗数、部分積 2 つ、積を上の桁からの列で返す。
crypt([A, B, C], [D, E], [F, G, H, I], [J, K, L], [M, N, O, P]) :-
    odd(A), even(B), even(C),
    even(E),
    mult([C, B, A], E, [I, H, G, F|X]),
    lefteven(F), odd(G), even(H), even(I), zero(X),
    lefteven(D),
    mult([C, B, A], D, [L, K, J|Y]),
    lefteven(J), odd(K), even(L), zero(Y),
    sum([I, H, G, F], [0, L, K, J], [P, O, N, M|Z]),
    odd(M), odd(N), even(O), even(P), zero(Z).

loop(_).
loop(N) :- N > 1, N1 is N - 1, loop(N1).

bench(N) :- loop(N), crypt(_, _, _, _, _), fail.
bench(_).

% 正しさ。唯一の解は 348 * 28 = 9744（部分積 2784 と 696）。
check :-
    crypt(Ds, Es, P1, P2, Ps),
    !,
    Ds == [3, 4, 8],
    Es == [2, 8],
    P1 == [2, 7, 8, 4],
    P2 == [6, 9, 6],
    Ps == [9, 7, 4, 4].

main :-
    check,
    current_prolog_flag(argv, [Arg|_]),
    atom_codes(Arg, Codes),
    number_codes(N, Codes),
    bench(N).
