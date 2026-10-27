:- module(apply, [
    maplist/2, maplist/3, maplist/4, maplist/5,
    foldl/4, foldl/5, foldl/6,
    include/3, exclude/3
]).

% 数字は後から足す引数の数。本体は apply なので、包まないと呼び出し元の述語が見えない。
:- meta_predicate
    maplist(1, ?),
    maplist(2, ?, ?),
    maplist(3, ?, ?, ?),
    maplist(4, ?, ?, ?, ?),
    foldl(3, ?, ?, ?),
    foldl(4, ?, ?, ?, ?),
    foldl(5, ?, ?, ?, ?, ?),
    include(1, ?, ?),
    exclude(1, ?, ?).

maplist(_, []).
maplist(Goal, [Head|Tail]) :- call(Goal, Head), maplist(Goal, Tail).

maplist(_, [], []).
maplist(Goal, [Head|Tail], [Out|Rest]) :-
    call(Goal, Head, Out),
    maplist(Goal, Tail, Rest).

maplist(_, [], [], []).
maplist(Goal, [A|As], [B|Bs], [C|Cs]) :-
    call(Goal, A, B, C),
    maplist(Goal, As, Bs, Cs).

maplist(_, [], [], [], []).
maplist(Goal, [A|As], [B|Bs], [C|Cs], [D|Ds]) :-
    call(Goal, A, B, C, D),
    maplist(Goal, As, Bs, Cs, Ds).

% 左の要素から畳む。先に再帰すると右畳みになる。
foldl(_, [], V, V).
foldl(Goal, [Head|Tail], V0, V) :-
    call(Goal, Head, V0, V1),
    foldl(Goal, Tail, V1, V).

foldl(_, [], [], V, V).
foldl(Goal, [A|As], [B|Bs], V0, V) :-
    call(Goal, A, B, V0, V1),
    foldl(Goal, As, Bs, V1, V).

foldl(_, [], [], [], V, V).
foldl(Goal, [A|As], [B|Bs], [C|Cs], V0, V) :-
    call(Goal, A, B, C, V0, V1),
    foldl(Goal, As, Bs, Cs, V1, V).

% 残すかは一度だけ決める。条件の選択点を残すと同じ要素が解を二つ出す。
include(_, [], []).
include(Goal, [Head|Tail], Out) :-
    (   call(Goal, Head)
    ->  Out = [Head|Rest]
    ;   Out = Rest
    ),
    include(Goal, Tail, Rest).

% 外すかも一度だけ決める。成功した条件の選択点を残すと空リストが解を二つ出す。
exclude(_, [], []).
exclude(Goal, [Head|Tail], Out) :-
    (   call(Goal, Head)
    ->  Out = Rest
    ;   Out = [Head|Rest]
    ),
    exclude(Goal, Tail, Rest).
