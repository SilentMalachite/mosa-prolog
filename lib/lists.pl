% library(lists)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_lists.pl。
% length/2 は ISO の核なので system にあり、ここでは公開し直すだけである。
:- module(lists, [append/3, member/2, memberchk/2, length/2, nth0/3, nth1/3, last/2,
                  reverse/2, msort/2, sum_list/2, max_list/2, list_to_set/2]).

append([], L, L).
append([H|T], L, [H|R]) :- append(T, L, R).

member(X, [X|_]).
member(X, [_|T]) :- member(X, T).

memberchk(X, L) :- member(X, L), !.

nth0(I, L, E) :- integer(I), !, I >= 0, nth_fixed(I, L, E).
nth0(I, L, E) :- var(I), !, nth_enum(L, E, 0, I).
nth0(I, _, _) :- throw(error(type_error(integer, I), context(nth0/3, _))).

nth1(I, L, E) :- integer(I), !, I >= 1, I0 is I - 1, nth_fixed(I0, L, E).
nth1(I, L, E) :- var(I), !, nth_enum(L, E, 1, I).
nth1(I, _, _) :- throw(error(type_error(integer, I), context(nth1/3, _))).

% 0 を頭の照合で選ぶと、頭が合わないとき次の節が負の添字で尾を辿り続ける。
nth_fixed(I, [H|T], E) :-
    (   I =:= 0
    ->  E = H
    ;   I1 is I - 1,
        nth_fixed(I1, T, E)
    ).

nth_enum([H|T], E, I0, I) :-
    (   I = I0,
        E = H
    ;   I1 is I0 + 1,
        nth_enum(T, E, I1, I)
    ).

last([X|Xs], Last) :- last_(Xs, X, Last).

last_([], Last, Last).
last_([X|Xs], _, Last) :- last_(Xs, X, Last).

reverse(L, R) :- reverse_(L, [], R).

reverse_([], R, R).
reverse_([X|Xs], A, R) :- reverse_(Xs, [X|A], R).

% 計画の Task 12 により keysort/2 に頼らない。並べた後で単一化するので、Sorted が
% 部分リストでも、合わない要素があっても、sort/2 と同じく失敗で済む。
msort(List, Sorted) :-
    must_be_list(List, msort/2),
    must_be_list_or_partial(Sorted, msort/2),
    merge_sort(List, Sorted0),
    Sorted = Sorted0.

% 巡回するリストを Prolog で辿ると止まらない。length/2 は巡回を type_error にする。
must_be_list(List, _) :- is_list(List), !.
must_be_list(List, PI) :-
    must_be_list_or_partial(List, PI),
    throw(error(instantiation_error, context(PI, _))).

% \+ と once/1 はゴールを項から組み直すので、巡回する項が切れる。節とカットで束縛を戻す。
must_be_list_or_partial(List, PI) :-
    catch(probe_length(List), error(Formal, _), throw(error(Formal, context(PI, _)))).
must_be_list_or_partial(_, _).

probe_length(List) :- length(List, _), !, fail.

merge_sort([], []) :- !.
merge_sort([X], [X]) :- !.
merge_sort(List, Sorted) :-
    halve(List, Left, Right),
    merge_sort(Left, SortedLeft),
    merge_sort(Right, SortedRight),
    merge(SortedLeft, SortedRight, Sorted).

halve([], [], []).
halve([X|Xs], [X|Left], Right) :- halve(Xs, Right, Left).

merge([], Right, Right) :- !.
merge(Left, [], Left) :- !.
merge([X|Xs], [Y|Ys], [Z|Zs]) :-
    (   X @=< Y
    ->  Z = X,
        merge(Xs, [Y|Ys], Zs)
    ;   Z = Y,
        merge([X|Xs], Ys, Zs)
    ).

sum_list(List, Sum) :- sum_list_(List, 0, Sum).

sum_list_([], Sum, Sum).
sum_list_([X|Xs], Sum0, Sum) :- Sum1 is Sum0 + X, sum_list_(Xs, Sum1, Sum).

max_list([X|Xs], Max) :- max_list_(Xs, X, Max).

max_list_([], Max, Max).
max_list_([X|Xs], Max0, Max) :- Max1 is max(Max0, X), max_list_(Xs, Max1, Max).

% 重複は == で見る。単一化で見ると、変数が後の要素と同じものとして消える。
% 部分リストを辿ると長い集合を無限に生成するので、msort/2 と同じく先に確かめる。
list_to_set(List, Set) :-
    must_be_list(List, list_to_set/2),
    list_to_set_(List, [], Set).

list_to_set_([], _, []).
list_to_set_([X|Xs], Seen, Set) :-
    (   seen(X, Seen)
    ->  Set = Set1
    ;   Set = [X|Set1]
    ),
    list_to_set_(Xs, [X|Seen], Set1).

seen(X, [Y|Ys]) :-
    (   X == Y
    ->  true
    ;   seen(X, Ys)
    ).
