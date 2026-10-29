% フェーズ6 Task 13。5 軒の家の謎（zebra）を、項の単一化とリストの後戻りで解く。探索は解が 1 つでも最後まで続く。
% それを失敗駆動で N 回。決まりは nrev.pl と同じ（N は旗 argv から、statistics/2 は使わない）。
% member/2 は library(lists) の述語なので、use_module なしで翻訳できるよう mem/2 を定義して使う。
:- initialization(main, main).

% house(色, 国, ペット, 飲み物, 煙草)
houses([
    house(_, norwegian, _, _, _),
    house(_, _, _, _, _),
    house(_, _, _, milk, _),
    house(_, _, _, _, _),
    house(_, _, _, _, _)
]).

mem(X, [X|_]).
mem(X, [_|T]) :- mem(X, T).

% right_of(A, B, Hs): A は B のすぐ右の家。
right_of(A, B, [B, A|_]).
right_of(A, B, [_|Hs]) :- right_of(A, B, Hs).

next_to(A, B, [A, B|_]).
next_to(A, B, [B, A|_]).
next_to(A, B, [_|Hs]) :- next_to(A, B, Hs).

zebra(Hs) :-
    houses(Hs),
    mem(house(red, english, _, _, _), Hs),
    mem(house(green, _, _, coffee, _), Hs),
    mem(house(_, spanish, dog, _, _), Hs),
    mem(house(yellow, _, _, _, kools), Hs),
    mem(house(_, ukrainian, _, tea, _), Hs),
    right_of(house(green, _, _, _, _), house(ivory, _, _, _, _), Hs),
    mem(house(_, _, snails, _, winstons), Hs),
    next_to(house(_, _, _, _, chesterfield), house(_, _, fox, _, _), Hs),
    next_to(house(_, _, _, _, kools), house(_, _, horse, _, _), Hs),
    mem(house(_, _, _, orange_juice, lucky_strike), Hs),
    mem(house(_, japanese, _, _, parliament), Hs),
    next_to(house(_, norwegian, _, _, _), house(blue, _, _, _, _), Hs),
    mem(house(_, _, zebra, _, _), Hs),
    mem(house(_, _, _, water, _), Hs).

loop(_).
loop(N) :- N > 1, N1 is N - 1, loop(N1).

bench(N) :- loop(N), zebra(_), fail.
bench(_).

% 正しさ。シマウマを飼うのは日本人、水を飲むのはノルウェー人（よく知られた唯一の解）。
check :-
    zebra(Hs),
    !,
    Hs == [house(yellow, norwegian, fox, water, kools),
           house(blue, ukrainian, horse, tea, chesterfield),
           house(red, english, snails, milk, winstons),
           house(ivory, spanish, dog, orange_juice, lucky_strike),
           house(green, japanese, zebra, coffee, parliament)].

main :-
    check,
    current_prolog_flag(argv, [Arg|_]),
    atom_codes(Arg, Codes),
    number_codes(N, Codes),
    bench(N).
