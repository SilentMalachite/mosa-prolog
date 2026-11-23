% library(apply)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_apply.pl。
% 閉包は meta_predicate で呼び手のモジュールに限定されるので、呼び手の非公開述語も渡せる。
% 宣言は述語ごとに 1 行ずつ書く。meta_predicate/1 はカンマで繋いだ並びも受けるが、1 行ずつのほうが探しやすい。
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

% リストを第1引数に置かず補助述語へ渡すのは、第1引数の索引でリストの頭を選ぶためである。リストが 2 つ以上なら、
% 初めの真のリストを先頭にした補助述語を選ぶ。真のリストで絞れば選択点が残らない（レビュー L-34）。どれも真のリスト
% でなければ、前と同じく第1のリストを先頭にする。補助述語は先頭のリストだけを入れ替え、G はいつも元の順で呼ぶ。
maplist(G, L) :- maplist_(L, G).

maplist_([], _).
maplist_([X|Xs], G) :- call(G, X), maplist_(Xs, G).

maplist(G, L1, L2) :-
    (   is_list(L1) -> maplist_(L1, L2, G)
    ;   is_list(L2) -> maplist_on2(L2, L1, G)
    ;   maplist_(L1, L2, G)
    ).

maplist_([], [], _).
maplist_([X|Xs], [Y|Ys], G) :- call(G, X, Y), maplist_(Xs, Ys, G).

maplist_on2([], [], _).
maplist_on2([Y|Ys], [X|Xs], G) :- call(G, X, Y), maplist_on2(Ys, Xs, G).

maplist(G, L1, L2, L3) :-
    (   is_list(L1) -> maplist_(L1, L2, L3, G)
    ;   is_list(L2) -> maplist_on2(L2, L1, L3, G)
    ;   is_list(L3) -> maplist_on3(L3, L1, L2, G)
    ;   maplist_(L1, L2, L3, G)
    ).

maplist_([], [], [], _).
maplist_([X|Xs], [Y|Ys], [Z|Zs], G) :- call(G, X, Y, Z), maplist_(Xs, Ys, Zs, G).

maplist_on2([], [], [], _).
maplist_on2([Y|Ys], [X|Xs], [Z|Zs], G) :- call(G, X, Y, Z), maplist_on2(Ys, Xs, Zs, G).

maplist_on3([], [], [], _).
maplist_on3([Z|Zs], [X|Xs], [Y|Ys], G) :- call(G, X, Y, Z), maplist_on3(Zs, Xs, Ys, G).

maplist(G, L1, L2, L3, L4) :-
    (   is_list(L1) -> maplist_(L1, L2, L3, L4, G)
    ;   is_list(L2) -> maplist_on2(L2, L1, L3, L4, G)
    ;   is_list(L3) -> maplist_on3(L3, L1, L2, L4, G)
    ;   is_list(L4) -> maplist_on4(L4, L1, L2, L3, G)
    ;   maplist_(L1, L2, L3, L4, G)
    ).

maplist_([], [], [], [], _).
maplist_([X|Xs], [Y|Ys], [Z|Zs], [W|Ws], G) :-
    call(G, X, Y, Z, W),
    maplist_(Xs, Ys, Zs, Ws, G).

maplist_on2([], [], [], [], _).
maplist_on2([Y|Ys], [X|Xs], [Z|Zs], [W|Ws], G) :-
    call(G, X, Y, Z, W),
    maplist_on2(Ys, Xs, Zs, Ws, G).

maplist_on3([], [], [], [], _).
maplist_on3([Z|Zs], [X|Xs], [Y|Ys], [W|Ws], G) :-
    call(G, X, Y, Z, W),
    maplist_on3(Zs, Xs, Ys, Ws, G).

maplist_on4([], [], [], [], _).
maplist_on4([W|Ws], [X|Xs], [Y|Ys], [Z|Zs], G) :-
    call(G, X, Y, Z, W),
    maplist_on4(Ws, Xs, Ys, Zs, G).

foldl(G, L, V0, V) :- foldl_(L, G, V0, V).

foldl_([], _, V, V).
foldl_([X|Xs], G, V0, V) :- call(G, X, V0, V1), foldl_(Xs, G, V1, V).

foldl(G, L1, L2, V0, V) :-
    (   is_list(L1) -> foldl_(L1, L2, G, V0, V)
    ;   is_list(L2) -> foldl_on2(L2, L1, G, V0, V)
    ;   foldl_(L1, L2, G, V0, V)
    ).

foldl_([], [], _, V, V).
foldl_([X|Xs], [Y|Ys], G, V0, V) :- call(G, X, Y, V0, V1), foldl_(Xs, Ys, G, V1, V).

foldl_on2([], [], _, V, V).
foldl_on2([Y|Ys], [X|Xs], G, V0, V) :- call(G, X, Y, V0, V1), foldl_on2(Ys, Xs, G, V1, V).

foldl(G, L1, L2, L3, V0, V) :-
    (   is_list(L1) -> foldl_(L1, L2, L3, G, V0, V)
    ;   is_list(L2) -> foldl_on2(L2, L1, L3, G, V0, V)
    ;   is_list(L3) -> foldl_on3(L3, L1, L2, G, V0, V)
    ;   foldl_(L1, L2, L3, G, V0, V)
    ).

foldl_([], [], [], _, V, V).
foldl_([X|Xs], [Y|Ys], [Z|Zs], G, V0, V) :-
    call(G, X, Y, Z, V0, V1),
    foldl_(Xs, Ys, Zs, G, V1, V).

foldl_on2([], [], [], _, V, V).
foldl_on2([Y|Ys], [X|Xs], [Z|Zs], G, V0, V) :-
    call(G, X, Y, Z, V0, V1),
    foldl_on2(Ys, Xs, Zs, G, V1, V).

foldl_on3([], [], [], _, V, V).
foldl_on3([Z|Zs], [X|Xs], [Y|Ys], G, V0, V) :-
    call(G, X, Y, Z, V0, V1),
    foldl_on3(Zs, Xs, Ys, G, V1, V).

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
