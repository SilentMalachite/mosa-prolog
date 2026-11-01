:- initialization(main, main).

% レビュー M1。-9223372036854775808 は min_integer の字面として読め、頭部と本体の両方で使える。
min_head(-9223372036854775808, head).

main :-
    X = -9223372036854775808,
    current_prolog_flag(min_integer, M),
    X == M,
    min_head(M, R),
    writeq([X, R, - 9223372036854775808]), nl.
