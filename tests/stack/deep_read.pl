% 深さ 1,000,000 の項を 1 MiB のスタックで read/1 で読む（ADR 0011 改訂 4）。small_stack.cmake が動かす。
% 各行は「形:結果」。多くの形は writeq/1 でファイルに書き、read/1 で読み戻して == で比べる。paren と bar は
% writeq では書けない（括弧は残らず、尾のリストは平らに書かれる）ので、字面を write_n/3 で書いて読む。
% 項は数え上げの繰り返しで作る。mosa の lib には numlist/3 が無い。
:- initialization(main, main).

depth(1000000).

% 関数表記の入れ子は f(f(a)) と書かれ、1 段ごとに引数の枠を積む。
nest_f(0, X, X) :- !.
nest_f(N, X, Y) :- M is N - 1, nest_f(M, f(X), Y).

% リストの入れ子は [[a]] と書かれ、1 段ごとにリストの枠を積む。
nest_list(0, X, X) :- !.
nest_list(N, X, Y) :- M is N - 1, nest_list(M, [X], Y).

% 前置演算子の入れ子は - - -a と書かれる。-(1) は - (1) と括弧付きで書かれ括弧の読みを通るので、葉はアトムにする。
nest_prefix(0, X, X) :- !.
nest_prefix(N, X, Y) :- M is N - 1, nest_prefix(M, -(X), Y).

% xfy の右の被演算子の入れ子は 3:2:1:a と括弧無しで書かれる。
nest_xfy(0, X, X) :- !.
nest_xfy(N, X, Y) :- M is N - 1, nest_xfy(M, N:X, Y).

% 波括弧の入れ子は {{a}} と書かれ、1 段ごとに波括弧の枠を積む。
nest_curly(0, X, X) :- !.
nest_curly(N, X, Y) :- M is N - 1, nest_curly(M, {X}, Y).

% [a|[a|b]] と同じ項。読むときは 1 段ごとに | の尾の枠を積む。
nest_bar(0, X, X) :- !.
nest_bar(N, X, Y) :- M is N - 1, nest_bar(M, [a|X], Y).

roundtrip(Name, T) :-
    open('deep_read.txt', write, W), writeq(W, T), write(W, '.'), nl(W), close(W),
    open('deep_read.txt', read, R), read(R, T2), close(R),
    ( T == T2 -> write(Name), write(':ok') ; write(Name), write(':different') ), nl.

write_n(0, _, _) :- !.
write_n(N, W, A) :- write(W, A), M is N - 1, write_n(M, W, A).

parens(N) :- open('deep_read.txt', write, W), write_n(N, W, '('), write(W, a), write_n(N, W, ')'), write(W, '.'), nl(W), close(W),
    open('deep_read.txt', read, R), read(R, T), close(R), ( T == a -> write('paren:ok') ; write('paren:different') ), nl.

bars(N) :- open('deep_read.txt', write, W), write_n(N, W, '[a|'), write(W, b), write_n(N, W, ']'), write(W, '.'), nl(W), close(W),
    open('deep_read.txt', read, R), read(R, T), close(R), nest_bar(N, b, B),
    ( T == B -> write('bar:ok') ; write('bar:different') ), nl.

% 形ごとに \+ \+ の中で動かす。抜けるときにその形の項をヒープから返し、次の形の項と同時に持たない。
main :- depth(D),
    \+ \+ ( nest_f(D, a, F), roundtrip(nested, F) ),
    \+ \+ ( nest_list(D, a, L), roundtrip(list, L) ),
    \+ \+ ( nest_prefix(D, a, P), roundtrip(prefix, P) ),
    \+ \+ ( nest_xfy(D, a, X), roundtrip(xfy, X) ),
    \+ \+ parens(D),
    \+ \+ ( nest_curly(D, a, C), roundtrip(curly, C) ),
    \+ \+ bars(D).
