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

nth0(Index, List, Element) :- nth_start(Index, List, Element, 0).
nth1(Index, List, Element) :- nth_start(Index, List, Element, 1).

% 整数の添字は先頭から数えて一度だけ辿る。開いたリストを列挙し続けない。
nth_start(Index, List, Element, Base) :-
    integer(Index), !,
    Index >= Base,
    Skip is Index - Base,
    nth_skip(Skip, List, Element).
nth_start(Index, List, Element, Base) :-
    var(Index), !,
    nth_from(Index, List, Element, Base).
nth_start(Index, _, _, _) :-
    throw(error(type_error(integer, Index), _)).

% 要素の照合は頭部でしない。合わなかったときに 0 を越えて辿らない。
nth_skip(0, List, Element) :- !, List = [Element|_].
nth_skip(Skip, [_|Tail], Element) :-
    Left is Skip - 1,
    nth_skip(Left, Tail, Element).

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

% 挿入整列はヒープを n^2 で使い、GC の無い今は数百要素で上限に届く。併合整列にする。
% 標準順序で等しい項は同一なので、分け方で順序が変わっても結果は同じ。
msort(List, Sorted) :- msort_run(List, Run), Sorted = Run.

% 結果は頭部で照合しない。照合に失敗すると次の節へ進み、空リストを分け続ける。
msort_run([], []) :- !.
msort_run([Element], [Element]) :- !.
msort_run(List, Sorted) :-
    msort_split(List, Left, Right),
    msort_run(Left, LeftSorted),
    msort_run(Right, RightSorted),
    msort_merge(LeftSorted, RightSorted, Sorted).

msort_split([], [], []).
msort_split([Head|Tail], [Head|Left], Right) :- msort_split(Tail, Right, Left).

msort_merge([], Right, Right) :- !.
msort_merge(Left, [], Left) :- !.
msort_merge([Left|Lefts], [Right|Rights], [Right|Merged]) :-
    Right @< Left, !,
    msort_merge([Left|Lefts], Rights, Merged).
msort_merge([Left|Lefts], Rights, [Left|Merged]) :-
    msort_merge(Lefts, Rights, Merged).

sum_list([], 0).
sum_list([Head|Tail], Sum) :- sum_list(Tail, Rest), Sum is Head + Rest.

max_list([Head|Tail], Max) :- max_acc(Tail, Head, Max).

max_acc([], Max, Max).
max_acc([Head|Tail], Acc, Max) :- Head > Acc, !, max_acc(Tail, Head, Max).
max_acc([_|Tail], Acc, Max) :- max_acc(Tail, Acc, Max).

% 最初に出た項を残す。同じかどうかは == で見る。member/2 の単一化は要素の変数を束縛する。
list_to_set(List, Set) :- set_seen(List, [], Set).

set_seen([], _, []).
set_seen([Head|Tail], Seen, Set) :- seen_identical(Head, Seen), !, set_seen(Tail, Seen, Set).
set_seen([Head|Tail], Seen, [Head|Set]) :- set_seen(Tail, [Head|Seen], Set).

seen_identical(Element, [Head|_]) :- Element == Head, !.
seen_identical(Element, [_|Tail]) :- seen_identical(Element, Tail).
