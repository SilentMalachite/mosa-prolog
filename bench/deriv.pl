% フェーズ6 Task 13。記号微分（Warren の deriv）。ops8、divide10、log10、times10 の 4 式を微分し、項を作っては
% 捨てる。それを失敗駆動で N 回。決まりは nrev.pl と同じ（N は旗 argv から、statistics/2 は使わない）。
% ^ は ISO の演算子表で xfy 200 なので、x^2 のまま書ける。
:- initialization(main, main).

d(U + V, X, DU + DV) :- !, d(U, X, DU), d(V, X, DV).
d(U - V, X, DU - DV) :- !, d(U, X, DU), d(V, X, DV).
d(U * V, X, DU * V + U * DV) :- !, d(U, X, DU), d(V, X, DV).
d(U / V, X, (DU * V - U * DV) / V ^ 2) :- !, d(U, X, DU), d(V, X, DV).
d(U ^ N, X, DU * N * U ^ N1) :- !, integer(N), N1 is N - 1, d(U, X, DU).
d(-U, X, -DU) :- !, d(U, X, DU).
d(exp(U), X, exp(U) * DU) :- !, d(U, X, DU).
d(log(U), X, DU / U) :- !, d(U, X, DU).
d(X, X, 1) :- !.
d(_, _, 0).

ops8(D) :- d((x + 1) * ((x ^ 2 + 2) * (x ^ 3 + 3)), x, D).
divide10(D) :- d(((((((((x / x) / x) / x) / x) / x) / x) / x) / x) / x, x, D).
log10(D) :- d(log(log(log(log(log(log(log(log(log(log(x)))))))))), x, D).
times10(D) :- d(((((((((x * x) * x) * x) * x) * x) * x) * x) * x) * x, x, D).

loop(_).
loop(N) :- N > 1, N1 is N - 1, loop(N1).

bench(N) :- loop(N), ops8(_), divide10(_), log10(_), times10(_), fail.
bench(_).

% 正しさ。ops8 の導関数を手で微分した形と比べ、times10 は 2 段だけの x * x * x で規則の組み合わせを見る。
check :-
    ops8(D),
    D == (1 + 0) * ((x ^ 2 + 2) * (x ^ 3 + 3)) +
         (x + 1) * ((1 * 2 * x ^ 1 + 0) * (x ^ 3 + 3) + (x ^ 2 + 2) * (1 * 3 * x ^ 2 + 0)),
    d(x * x * x, x, D3),
    D3 == (1 * x + x * 1) * x + x * x * 1,
    log10(_),
    divide10(_),
    times10(_).

main :-
    check,
    current_prolog_flag(argv, [Arg|_]),
    atom_codes(Arg, Codes),
    number_codes(N, Codes),
    bench(N).
