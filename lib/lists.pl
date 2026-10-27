:- module(lists, [
    append/3,
    member/2,
    memberchk/2,
    length/2,
    nth0/3,
    nth1/3,
    last/2,
    reverse/2,
    msort/2,
    sum_list/2,
    max_list/2,
    list_to_set/2
]).

% length/2 は system の組込。このモジュールは再公開するだけで、節は持たない。

append([], List, List).
append([Head|Tail], List, [Head|Rest]) :- append(Tail, List, Rest).

member(Element, [Element|_]).
member(Element, [_|Rest]) :- member(Element, Rest).

% 最初の解で切る。続きの member は試さない。
memberchk(Element, List) :- member(Element, List), !.

nth0(Index, List, Element) :- nth_from(Index, List, Element, 0).
nth1(Index, List, Element) :- nth_from(Index, List, Element, 1).

% 見た個数と添字が一致した要素を返す。添字が変数なら先頭から順に出す。
nth_from(Index, [Element|_], Element, Index).
nth_from(Index, [_|Tail], Element, Seen) :-
    Next is Seen + 1,
    nth_from(Index, Tail, Element, Next).

last([Element], Element).
last([_|Tail], Element) :- last(Tail, Element).

% 補助は公開しない。節の本体は lists を呼び出しモジュールにして探す。
reverse(List, Reversed) :- reverse_acc(List, [], Reversed).

reverse_acc([], Acc, Acc).
reverse_acc([Head|Tail], Acc, Reversed) :- reverse_acc(Tail, [Head|Acc], Reversed).

% 先に現れた同じ項を前に残す。より小さい項だけを通り過ぎて挿す。
msort([], []).
msort([Head|Tail], Sorted) :- msort(Tail, Rest), msort_insert(Head, Rest, Sorted).

msort_insert(Element, [], [Element]).
msort_insert(Element, [Head|Tail], [Head|Rest]) :-
    Head @< Element, !,
    msort_insert(Element, Tail, Rest).
msort_insert(Element, List, [Element|List]).

sum_list([], 0).
sum_list([Head|Tail], Sum) :- sum_list(Tail, Rest), Sum is Head + Rest.

max_list([Head|Tail], Max) :- max_acc(Tail, Head, Max).

max_acc([], Max, Max).
max_acc([Head|Tail], Acc, Max) :- Head > Acc, !, max_acc(Tail, Head, Max).
max_acc([_|Tail], Acc, Max) :- max_acc(Tail, Acc, Max).

% 最初に出た項を残す。
list_to_set(List, Set) :- set_seen(List, [], Set).

set_seen([], _, []).
set_seen([Head|Tail], Seen, Set) :- member(Head, Seen), !, set_seen(Tail, Seen, Set).
set_seen([Head|Tail], Seen, [Head|Set]) :- set_seen(Tail, [Head|Seen], Set).
