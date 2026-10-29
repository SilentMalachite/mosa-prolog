% フェーズ6 Task 6。SPEC 第8節の nrev（naive reverse）。30 要素のリストを失敗駆動で N 回逆順にする。
% 時間は mosatest --bench が外から壁時計で測る（statistics/2 はフェーズ7）。インタプリタでは
% `mosa bench/nrev.pl -- N`、翻訳した実行ファイルでは `nrev N` で、どちらも旗 argv から N を読む。
% 繰り返しは失敗駆動にする。1<<20 語のヒープでは決定的な繰り返しがすぐ上限に届く（計画の注）。
% append/3 ではなく app/3 を定義して使う。append/3 は library(lists) の述語で、use_module なしでは未定義の呼び出しに
% なり mosac emit が止まる。ADR 0009 の下見（手書きの nrev/app）と同じ述語にして、その数と比べられるようにする。
:- initialization(main, main).

app([], L, L).
app([H|T], L, [H|R]) :- app(T, L, R).

nrev([], []).
nrev([H|T], R) :- nrev(T, S), app(S, [H], R).

range(N, N, [N]) :- !.
range(I, N, [I|T]) :- I < N, I1 is I + 1, range(I1, N, T).

loop(_).
loop(N) :- N > 1, N1 is N - 1, loop(N1).

bench(N) :- range(1, 30, L), loop(N), nrev(L, _), fail.
bench(_).

% 正しさ。最初の解が逆順のリストと一致しなければ main が失敗し、終了コード 1 になる。
check :-
    range(1, 30, L),
    nrev(L, R),
    R == [30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
          15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1].

main :-
    check,
    current_prolog_flag(argv, [Arg|_]),
    atom_codes(Arg, Codes),
    number_codes(N, Codes),
    bench(N).
