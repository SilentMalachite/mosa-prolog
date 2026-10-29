% フェーズ6 Task 13。8 王妃のすべての解（92 個）を、置くたびに利きを確かめる生成で探す。それを失敗駆動で N 回。
% 決まりは nrev.pl と同じ。時間は mosatest --bench が外から測り、N は旗 argv から読み、statistics/2 は使わない。
% select/3 と between/3 は library の述語なので、use_module なしで翻訳できるよう sel/3 と range/3 を定義して使う。
:- initialization(main, main).

range(N, N, [N]) :- !.
range(I, N, [I|T]) :- I < N, I1 is I + 1, range(I1, N, T).

sel(X, [X|T], T).
sel(X, [H|T], [H|R]) :- sel(X, T, R).

% Safe は置いた王妃を新しい順に持つ。D 列離れた王妃と斜めに並べば利きがある。
attack(Q, Safe) :- attack(Q, 1, Safe).
attack(Q, D, [Q1|_]) :- Q =:= Q1 + D.
attack(Q, D, [Q1|_]) :- Q =:= Q1 - D.
attack(Q, D, [_|Qs]) :- D1 is D + 1, attack(Q, D1, Qs).

place([], Qs, Qs).
place(Unplaced, Safe, Qs) :-
    sel(Q, Unplaced, Rest),
    \+ attack(Q, Safe),
    place(Rest, [Q|Safe], Qs).

queens(N, Qs) :- range(1, N, Ns), place(Ns, [], Qs).

loop(_).
loop(N) :- N > 1, N1 is N - 1, loop(N1).

bench(N) :- loop(N), queens(8, _), fail.
bench(_).

% 正しさ。辞書順で最初の解は 1 列目から 1,5,8,6,3,7,2,4 で、Qs は最後に置いた王妃から並ぶ。
check :-
    queens(8, Qs),
    !,
    Qs == [4, 2, 7, 3, 6, 8, 5, 1].

main :-
    check,
    current_prolog_flag(argv, [Arg|_]),
    atom_codes(Arg, Codes),
    number_codes(N, Codes),
    bench(N).
