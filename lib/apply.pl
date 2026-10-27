:- module(apply, [maplist/2, maplist/3, maplist/4, maplist/5]).

% 数字は後から足す引数の数。本体は apply なので、包まないと呼び出し元の述語が見えない。
:- meta_predicate
    maplist(1, ?),
    maplist(2, ?, ?),
    maplist(3, ?, ?, ?),
    maplist(4, ?, ?, ?, ?).

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
