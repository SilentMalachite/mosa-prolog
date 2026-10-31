% 入れ子の収集（findall/3、findall/4、bagof/3、setof/3）を、1 MiB のスタックで 100,000 段動かす（ADR 0013、
% レビュー H4）。各段は 1 つ下の段の答え x を 1 要素のリストで受け取る。答えの項を段ごとに伸ばすと、写すだけで
% 段数の 2 乗の時間になるので、伸ばす形（レビューの再現 d/2）は 3,000 段で確かめる。書き直す前は 1 MiB で
% 1,000 段に届かずに落ちた。各行は「名前: 結果」。一番下の throw は一番上の catch まで届くこと。
:- initialization(main, main).

depth(100000).

fa(0) :- !.
fa(N) :- M is N - 1, findall(x, fa(M), [x]).

f4(0) :- !.
f4(N) :- M is N - 1, findall(x, f4(M), [x], []).

bo(0) :- !.
bo(N) :- M is N - 1, bagof(x, bo(M), [x]).

so(0) :- !.
so(N) :- M is N - 1, setof(x, so(M), [x]).

% 自由変数 W があるので、どの段も証人で解を分ける bagof の道を通る。
bw(0, w) :- !.
bw(N, W) :- M is N - 1, bagof(x, bw(M, W), [x]).

th(0) :- throw(bottom).
th(N) :- M is N - 1, findall(x, th(M), _).

d(0, z) :- !.
d(N, s(L)) :- M is N - 1, findall(X, d(M, X), [L]).

sdepth(T, N) :- sdepth(T, 0, N).
sdepth(z, N, N).
sdepth(s(T), N0, N) :- N1 is N0 + 1, sdepth(T, N1, N).

% 各行は \+ \+ で束縛を戻し、前の行の深い項を次の行へ持ち越さない。
show(Name, Goal) :-
    (   \+ \+ catch(Goal, E, (write(Name: E), nl, fail))
    ->  write(Name: ok), nl
    ;   write(Name: failed), nl
    ).

main :-
    depth(D),
    show(findall, fa(D)),
    show(findall4, f4(D)),
    show(bagof, bo(D)),
    show(setof, so(D)),
    show(bagof_witness, (bw(D, W), W == w)),
    show(throw_bottom, catch(th(D), bottom, true)),
    show(review_repro, (d(3000, T), sdepth(T, 3000))).
