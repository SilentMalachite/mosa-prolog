% library(ordsets)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_ordsets.pl。
% 名前と引数の順は SWI-Prolog のマニュアル（10.1.20、2026-11-16 に読んだ）の ordsets の節に合わせ、コードは新しく書いた。
% 順序は compare/3 の標準順序で、集合は重なりのない昇順のリスト。マニュアルに誤りの記述は無いので、誤りは足さない。
:- module(ordsets, [is_ordset/1, ord_empty/1, ord_seteq/2, list_to_ord_set/2,
                    ord_intersect/2, ord_disjoint/2,
                    ord_intersection/2, ord_intersection/3, ord_intersection/4,
                    ord_add_element/3, ord_del_element/3, ord_selectchk/3, ord_memberchk/2,
                    ord_range/4, ord_subset/2, ord_subtract/3,
                    ord_union/2, ord_union/3, ord_union/4, ord_symdiff/3]).

% 二つの集合を先頭どうしで比べて進める述語は、比べた結果を 1 回の if-then-else で分け、
% 選択点を残さない。

is_ordset(Set) :-
    nonvar(Set),
    (   Set == []
    ->  true
    ;   Set = [First|Rest],
        rising_after(Rest, First)
    ).

rising_after(Tail, Prev) :-
    nonvar(Tail),
    (   Tail == []
    ->  true
    ;   Tail = [Next|More],
        Prev @< Next,
        rising_after(More, Next)
    ).

ord_empty([]).

ord_seteq(Set1, Set2) :-
    Set1 == Set2.

list_to_ord_set(List, Set) :-
    sort(List, Set).

ord_intersect(Set1, Set2) :-
    Set1 = [X|Xs],
    Set2 = [Y|Ys],
    compare(Order, X, Y),
    (   Order = (=)
    ->  true
    ;   Order = (<)
    ->  ord_intersect(Xs, Set2)
    ;   ord_intersect(Set1, Ys)
    ).

ord_disjoint(Set1, Set2) :-
    \+ ord_intersect(Set1, Set2).

% 空の集合の集まりには共通部分が決まらないので失敗する。
ord_intersection([First|Sets], Common) :-
    intersect_all(Sets, First, Common).

intersect_all([], Acc, Acc).
intersect_all([Set|Sets], Acc, Common) :-
    ord_intersection(Acc, Set, Next),
    intersect_all(Sets, Next, Common).

ord_intersection(Set1, Set2, Common) :-
    (   Set1 = [X|Xs],
        Set2 = [Y|Ys]
    ->  compare(Order, X, Y),
        (   Order = (=)
        ->  Common = [X|More],
            ord_intersection(Xs, Ys, More)
        ;   Order = (<)
        ->  ord_intersection(Xs, Set2, Common)
        ;   ord_intersection(Set1, Ys, Common)
        )
    ;   Common = []
    ).

% 4 つ目は Set1 に無い Set2 の要素。
ord_intersection(Set1, Set2, Common, Extra) :-
    ord_intersection(Set1, Set2, Common),
    ord_subtract(Set2, Set1, Extra).

ord_add_element(Set, Element, NewSet) :-
    ord_union(Set, [Element], NewSet).

ord_del_element(Set, Element, NewSet) :-
    ord_subtract(Set, [Element], NewSet).

% 見つからなければ失敗する。要素より大きいところまで進んだら探すのをやめる。
ord_selectchk(Element, Set, Rest) :-
    Set = [Head|Tail],
    compare(Order, Element, Head),
    (   Order = (=)
    ->  Rest = Tail
    ;   Order = (>),
        Rest = [Head|More],
        ord_selectchk(Element, Tail, More)
    ).

ord_memberchk(Element, Set) :-
    Set = [Head|Tail],
    compare(Order, Element, Head),
    (   Order = (=)
    ->  true
    ;   Order = (>),
        ord_memberchk(Element, Tail)
    ).

% Low と High の両端を含む。標準順序で Low より前の要素を捨ててから、High を超えるまで取る。
ord_range(Set, Low, High, Range) :-
    drop_below(Set, Low, Rest),
    take_through(Rest, High, Range).

drop_below(Set, Low, Rest) :-
    (   Set = [X|Tail],
        X @< Low
    ->  drop_below(Tail, Low, Rest)
    ;   Rest = Set
    ).

take_through(Set, High, Range) :-
    (   Set = [X|Tail],
        X @=< High
    ->  Range = [X|More],
        take_through(Tail, High, More)
    ;   Range = []
    ).

ord_subset(Sub, Super) :-
    (   Sub = [X|Xs]
    ->  Super = [Y|Ys],
        compare(Order, X, Y),
        (   Order = (=)
        ->  ord_subset(Xs, Ys)
        ;   Order = (>),
            ord_subset(Sub, Ys)
        )
    ;   Sub == []
    ).

ord_subtract(Set, Remove, Rest) :-
    (   Set = [X|Xs]
    ->  (   Remove = [Y|Ys]
        ->  compare(Order, X, Y),
            (   Order = (=)
            ->  ord_subtract(Xs, Ys, Rest)
            ;   Order = (<)
            ->  Rest = [X|More],
                ord_subtract(Xs, Remove, More)
            ;   ord_subtract(Set, Ys, Rest)
            )
        ;   Rest = Set
        )
    ;   Rest = []
    ).

ord_union([], []).
ord_union([First|Sets], Union) :-
    union_all(Sets, First, Union).

union_all([], Acc, Acc).
union_all([Set|Sets], Acc, Union) :-
    ord_union(Acc, Set, Next),
    union_all(Sets, Next, Union).

ord_union(Set1, Set2, Union) :-
    (   Set1 = [X|Xs]
    ->  (   Set2 = [Y|Ys]
        ->  compare(Order, X, Y),
            (   Order = (=)
            ->  Union = [X|More],
                ord_union(Xs, Ys, More)
            ;   Order = (<)
            ->  Union = [X|More],
                ord_union(Xs, Set2, More)
            ;   Union = [Y|More],
                ord_union(Set1, Ys, More)
            )
        ;   Union = Set1
        )
    ;   Union = Set2
    ).

% 4 つ目は Set1 に無い Set2 の要素。
ord_union(Set1, Set2, Union, New) :-
    ord_union(Set1, Set2, Union),
    ord_subtract(Set2, Set1, New).

ord_symdiff(Set1, Set2, Diff) :-
    (   Set1 = [X|Xs]
    ->  (   Set2 = [Y|Ys]
        ->  compare(Order, X, Y),
            (   Order = (=)
            ->  ord_symdiff(Xs, Ys, Diff)
            ;   Order = (<)
            ->  Diff = [X|More],
                ord_symdiff(Xs, Set2, More)
            ;   Diff = [Y|More],
                ord_symdiff(Set1, Ys, More)
            )
        ;   Diff = Set1
        )
    ;   Diff = Set2
    ).
