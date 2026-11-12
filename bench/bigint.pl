% フェーズ10 タスク 12。任意精度の整数（Big、ADR 0025）の費用を測る。1 回に 7^7^7 を作ってその msb を取り、
% 1000! を掛け算で作って number_codes で文字にする。それを失敗駆動で N 回。決まりは nrev.pl と同じ（N は旗 argv から、
% statistics/2 は使わない）。7^7^7 は文字にしない。to_text が 2 乗の時間（約 12 秒）で、変換の費用だけを測ることになる。
% 1000! は 2568 桁なので文字にしても軽い。1 回ごとに掛け算を 1000 回（21! から先は Big）と、文字への変換を 1 回通る。
:- initialization(main, main).

fact(0, 1) :- !.
fact(N, F) :- N1 is N - 1, fact(N1, F1), F is N * F1.

% 数字の文字の和。期待する値は Python の sum(map(int, str(math.factorial(1000)))) で求めた。
digit_sum([], S, S).
digit_sum([C|Cs], S0, S) :- S1 is S0 + C - 0'0, digit_sum(Cs, S1, S).

loop(_).
loop(N) :- N > 1, N1 is N - 1, loop(N1).

bench(N) :- loop(N), X is 7^7^7, _ is msb(X), fact(1000, F), number_codes(F, _), fail.
bench(_).

% 正しさ。7^7^7 の最上位の 1 は 0 から数えて 2311977 ビット目（Python の (7**7**7).bit_length() - 1）、1000! は 2568 桁で
% 数字の和は 10539。
check :-
    B is msb(7^7^7),
    B == 2311977,
    fact(1000, F),
    number_codes(F, Cs),
    length(Cs, 2568),
    digit_sum(Cs, 0, 10539).

main :-
    check,
    current_prolog_flag(argv, [Arg|_]),
    atom_codes(Arg, Codes),
    number_codes(N, Codes),
    bench(N).
