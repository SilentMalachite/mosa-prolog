% library(apply)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_apply.pl。
% 閉包は meta_predicate で呼び手のモジュールに限定されるので、呼び手の非公開述語も渡せる。
% 宣言は 1 つずつ書く。meta_predicate/1 はカンマで繋いだ並びを受けない。
:- module(apply, [maplist/2, maplist/3, maplist/4, maplist/5,
                  foldl/4, foldl/5, foldl/6, include/3, exclude/3]).

:- meta_predicate maplist(1, ?).
:- meta_predicate maplist(2, ?, ?).
:- meta_predicate maplist(3, ?, ?, ?).
:- meta_predicate maplist(4, ?, ?, ?, ?).
:- meta_predicate foldl(3, ?, ?, ?).
:- meta_predicate foldl(4, ?, ?, ?, ?).
:- meta_predicate foldl(5, ?, ?, ?, ?, ?).
:- meta_predicate include(1, ?, ?).
:- meta_predicate exclude(1, ?, ?).

% リストを第1引数に置かず補助述語へ渡すのは、第1引数の索引でリストの頭を選ぶためである。
maplist(G, L) :- maplist_(L, G).

maplist_([], _).
maplist_([X|Xs], G) :- call(G, X), maplist_(Xs, G).

maplist(G, L1, L2) :- maplist_(L1, L2, G).

maplist_([], [], _).
maplist_([X|Xs], [Y|Ys], G) :- call(G, X, Y), maplist_(Xs, Ys, G).

maplist(G, L1, L2, L3) :- maplist_(L1, L2, L3, G).

maplist_([], [], [], _).
maplist_([X|Xs], [Y|Ys], [Z|Zs], G) :- call(G, X, Y, Z), maplist_(Xs, Ys, Zs, G).

maplist(G, L1, L2, L3, L4) :- maplist_(L1, L2, L3, L4, G).

maplist_([], [], [], [], _).
maplist_([X|Xs], [Y|Ys], [Z|Zs], [W|Ws], G) :-
    call(G, X, Y, Z, W),
    maplist_(Xs, Ys, Zs, Ws, G).

foldl(G, L, V0, V) :- foldl_(L, G, V0, V).

foldl_([], _, V, V).
foldl_([X|Xs], G, V0, V) :- call(G, X, V0, V1), foldl_(Xs, G, V1, V).

foldl(G, L1, L2, V0, V) :- foldl_(L1, L2, G, V0, V).

foldl_([], [], _, V, V).
foldl_([X|Xs], [Y|Ys], G, V0, V) :- call(G, X, Y, V0, V1), foldl_(Xs, Ys, G, V1, V).

foldl(G, L1, L2, L3, V0, V) :- foldl_(L1, L2, L3, G, V0, V).

foldl_([], [], [], _, V, V).
foldl_([X|Xs], [Y|Ys], [Z|Zs], G, V0, V) :-
    call(G, X, Y, Z, V0, V1),
    foldl_(Xs, Ys, Zs, G, V1, V).

% 要素ごとに一度だけ選ぶ。G が複数の解を持っても結果のリストは一通りにする。
include(G, L, In) :- include_(L, G, In).

include_([], _, []).
include_([X|Xs], G, In) :-
    (   call(G, X)
    ->  In = [X|In1]
    ;   In = In1
    ),
    include_(Xs, G, In1).

exclude(G, L, Out) :- exclude_(L, G, Out).

exclude_([], _, []).
exclude_([X|Xs], G, Out) :-
    (   call(G, X)
    ->  Out = Out1
    ;   Out = [X|Out1]
    ),
    exclude_(Xs, G, Out1).
