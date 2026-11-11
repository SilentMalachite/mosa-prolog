% 深さ 1,000,000 の項を writeq/1 でファイルに書き、read/1 で読み戻して == で比べる（ADR 0011 改訂 4）。
% small_stack.cmake が 1 MiB のスタックで動かす。各行は「形:結果」。
% 項は数え上げの繰り返しで作る。mosa の lib には numlist/3 が無い。
:- initialization(main, main).

depth(1000000).

% 前置演算子の入れ子は - - -a と書かれる。-(1) は - (1) と括弧付きで書かれ括弧の読みを通るので、葉はアトムにする。
nest_prefix(0, X, X) :- !.
nest_prefix(N, X, Y) :- M is N - 1, nest_prefix(M, -(X), Y).

% xfy の右の被演算子の入れ子は 3:2:1:a と括弧無しで書かれる。
nest_xfy(0, X, X) :- !.
nest_xfy(N, X, Y) :- M is N - 1, nest_xfy(M, N:X, Y).

roundtrip(Name, T) :-
    open('deep_read.txt', write, W), writeq(W, T), write(W, '.'), nl(W), close(W),
    open('deep_read.txt', read, R), read(R, T2), close(R),
    ( T == T2 -> write(Name), write(':ok') ; write(Name), write(':different') ), nl.

main :- depth(D), nest_prefix(D, a, P), roundtrip(prefix, P), nest_xfy(D, a, X), roundtrip(xfy, X).
