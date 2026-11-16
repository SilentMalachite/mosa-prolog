% library(assoc)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_assoc.pl。
% 名前と引数の順と誤りは SWI-Prolog のマニュアル（10.1.20、2026-11-16 に読んだ）の assoc の節に合わせ、コードは新しく書いた。
% 木は t（空）と t(Key, Value, Balance, Left, Right) の AVL 木で、Balance は compare(Balance, 左の高さ, 右の高さ) の結果。
% 空の連想配列への del_min_assoc/4 と del_max_assoc/4 は失敗する（iso-notes の library(assoc) の節）。
:- module(assoc, [empty_assoc/1, list_to_assoc/2, ord_list_to_assoc/2,
                  get_assoc/3, get_assoc/5, max_assoc/3, min_assoc/3, gen_assoc/3,
                  put_assoc/4, del_assoc/4, del_min_assoc/4, del_max_assoc/4,
                  assoc_to_list/2, assoc_to_keys/2, assoc_to_values/2,
                  is_assoc/1, map_assoc/2, map_assoc/3]).

:- meta_predicate map_assoc(1, ?), map_assoc(2, ?, ?).

empty_assoc(t).

% ---- 作る ----

% 鍵が重なれば、リスト全体を添えて誤りにする。
list_to_assoc(List, Assoc) :-
    add_pairs(List, List, t, Assoc).

add_pairs([], _, Assoc, Assoc).
add_pairs([Key-Value|Pairs], Whole, Assoc0, Assoc) :-
    (   get_assoc(Key, Assoc0, _)
    ->  throw(error(domain_error(unique_key_pairs, Whole), context(list_to_assoc/2, _)))
    ;   put_assoc(Key, Assoc0, Value, Assoc1),
        add_pairs(Pairs, Whole, Assoc1, Assoc)
    ).

% 昇順と分かっているので、挿入せずに左右の大きさが 1 以下の差の木を直に組む。
% 左の大きさ (N-1)//2、右の大きさが残りなら、木の高さは大きさに対して増えるだけなので、左は右より低いか等しい。
ord_list_to_assoc(List, Assoc) :-
    (   keys_rise(List)
    ->  length(List, Count),
        build_balanced(Count, List, _, Assoc, _)
    ;   throw(error(domain_error(key_ordered_pairs, List), context(ord_list_to_assoc/2, _)))
    ).

keys_rise([]).
keys_rise([Key-_|Pairs]) :-
    keys_rise_after(Pairs, Key).

keys_rise_after([], _).
keys_rise_after([Next-_|Pairs], Prev) :-
    Prev @< Next,
    keys_rise_after(Pairs, Next).

build_balanced(0, Pairs, Pairs, t, 0) :- !.
build_balanced(Count, Pairs0, Rest, t(Key, Value, Balance, Left, Right), Height) :-
    LeftCount is (Count - 1) // 2,
    RightCount is Count - 1 - LeftCount,
    build_balanced(LeftCount, Pairs0, [Key-Value|Pairs1], Left, LeftHeight),
    build_balanced(RightCount, Pairs1, Rest, Right, RightHeight),
    compare(Balance, LeftHeight, RightHeight),
    Height is max(LeftHeight, RightHeight) + 1.

% ---- 引く ----

get_assoc(Key, Assoc, Value) :-
    Assoc = t(Key0, Value0, _, Left, Right),
    compare(Order, Key, Key0),
    (   Order = (=)
    ->  Value = Value0
    ;   Order = (<)
    ->  get_assoc(Key, Left, Value)
    ;   get_assoc(Key, Right, Value)
    ).

% 値を Old から New に替えた連想配列を Assoc とする。
get_assoc(Key, Assoc0, Old, Assoc, New) :-
    Assoc0 = t(Key0, Value0, Balance, Left0, Right0),
    compare(Order, Key, Key0),
    (   Order = (=)
    ->  Old = Value0,
        Assoc = t(Key0, New, Balance, Left0, Right0)
    ;   Order = (<)
    ->  get_assoc(Key, Left0, Old, Left, New),
        Assoc = t(Key0, Value0, Balance, Left, Right0)
    ;   get_assoc(Key, Right0, Old, Right, New),
        Assoc = t(Key0, Value0, Balance, Left0, Right)
    ).

max_assoc(t(Key0, Value0, _, _, Right), Key, Value) :-
    (   Right == t
    ->  Key = Key0,
        Value = Value0
    ;   max_assoc(Right, Key, Value)
    ).

min_assoc(t(Key0, Value0, _, Left, _), Key, Value) :-
    (   Left == t
    ->  Key = Key0,
        Value = Value0
    ;   min_assoc(Left, Key, Value)
    ).

% 鍵が基底なら引くだけにして選択点を残さない。そうでなければ昇順に 1 組ずつ返す。
gen_assoc(Key, Assoc, Value) :-
    (   ground(Key)
    ->  get_assoc(Key, Assoc, Value)
    ;   in_order(Key, Assoc, Value)
    ).

in_order(Key, t(Key0, Value0, _, Left, Right), Value) :-
    (   in_order(Key, Left, Value)
    ;   Key = Key0,
        Value = Value0
    ;   in_order(Key, Right, Value)
    ).

% ---- 木の組み直し ----
% 部分木の高さが 1 だけ増えた、または減ったときの親の直し方は、高さの差の算数から導く。
% 差が 2 になった節は、重い側の子の Balance で単回転か二重回転かを選ぶ。

% 左の高さが 2 差になった節（左が重い）を回し、回した後の木と、高さが減ったかを返す。
% 単回転は子の傾きが同じ側のとき、二重回転は逆の側のとき。傾きが無い子は削除のときだけ来る。
tilt_right(Key, Value, t(KeyL, ValueL, BalanceL, Far, Near), Right, Tree, Shrunk) :-
    (   BalanceL = (>)
    ->  Tree = t(KeyL, ValueL, =, Far, t(Key, Value, =, Near, Right)),
        Shrunk = yes
    ;   BalanceL = (=)
    ->  Tree = t(KeyL, ValueL, <, Far, t(Key, Value, >, Near, Right)),
        Shrunk = no
    ;   Near = t(KeyM, ValueM, BalanceM, MidLeft, MidRight),
        pivot_balances(BalanceM, BalanceA, BalanceB),
        Tree = t(KeyM, ValueM, =,
                 t(KeyL, ValueL, BalanceA, Far, MidLeft),
                 t(Key, Value, BalanceB, MidRight, Right)),
        Shrunk = yes
    ).

% tilt_right の鏡像。右の高さが 2 差になった節（右が重い）を回す。
tilt_left(Key, Value, Left, t(KeyR, ValueR, BalanceR, Near, Far), Tree, Shrunk) :-
    (   BalanceR = (<)
    ->  Tree = t(KeyR, ValueR, =, t(Key, Value, =, Left, Near), Far),
        Shrunk = yes
    ;   BalanceR = (=)
    ->  Tree = t(KeyR, ValueR, >, t(Key, Value, <, Left, Near), Far),
        Shrunk = no
    ;   Near = t(KeyM, ValueM, BalanceM, MidLeft, MidRight),
        pivot_balances(BalanceM, BalanceA, BalanceB),
        Tree = t(KeyM, ValueM, =,
                 t(Key, Value, BalanceA, Left, MidLeft),
                 t(KeyR, ValueR, BalanceB, MidRight, Far)),
        Shrunk = yes
    ).

% 二重回転の軸になった節の傾きから、分かれた左右の節の傾きを決める。
% 軸の左が高ければ（>）、分かれた左は釣り合い、右は左より低くなる。逆の側はその鏡像。
pivot_balances(=, =, =).
pivot_balances(>, =, <).
pivot_balances(<, >, =).

% ---- 入れる ----

put_assoc(Key, Assoc0, Value, Assoc) :-
    insert_node(Assoc0, Key, Value, Assoc, _).

% 4 つ目の引数の木が元より高くなったかを最後の引数で返す。
insert_node(Assoc0, Key, Value, Assoc, Grew) :-
    (   Assoc0 == t
    ->  Assoc = t(Key, Value, =, t, t),
        Grew = yes
    ;   Assoc0 = t(Key0, Value0, Balance, Left, Right),
        compare(Order, Key, Key0),
        insert_by_order(Order, Key, Value, Key0, Value0, Balance, Left, Right, Assoc, Grew)
    ).

insert_by_order(=, _, Value, Key0, _, Balance, Left, Right, t(Key0, Value, Balance, Left, Right), no).
insert_by_order(<, Key, Value, Key0, Value0, Balance, Left0, Right, Assoc, Grew) :-
    insert_node(Left0, Key, Value, Left, LeftGrew),
    (   LeftGrew = no
    ->  Assoc = t(Key0, Value0, Balance, Left, Right),
        Grew = no
    ;   left_grew(Balance, Key0, Value0, Left, Right, Assoc, Grew)
    ).
insert_by_order(>, Key, Value, Key0, Value0, Balance, Left, Right0, Assoc, Grew) :-
    insert_node(Right0, Key, Value, Right, RightGrew),
    (   RightGrew = no
    ->  Assoc = t(Key0, Value0, Balance, Left, Right),
        Grew = no
    ;   right_grew(Balance, Key0, Value0, Left, Right, Assoc, Grew)
    ).

% 左が 1 高くなった。右が高かったなら釣り合い、釣り合っていたなら左が重くなって全体も高くなる。
% 左が重かったなら差が 2 になるので回す。入れた後の回転は、回す前の高さに戻る。
left_grew(Balance, Key, Value, Left, Right, Assoc, Grew) :-
    (   Balance = (<)
    ->  Assoc = t(Key, Value, =, Left, Right),
        Grew = no
    ;   Balance = (=)
    ->  Assoc = t(Key, Value, >, Left, Right),
        Grew = yes
    ;   tilt_right(Key, Value, Left, Right, Assoc, _),
        Grew = no
    ).

right_grew(Balance, Key, Value, Left, Right, Assoc, Grew) :-
    (   Balance = (>)
    ->  Assoc = t(Key, Value, =, Left, Right),
        Grew = no
    ;   Balance = (=)
    ->  Assoc = t(Key, Value, <, Left, Right),
        Grew = yes
    ;   tilt_left(Key, Value, Left, Right, Assoc, _),
        Grew = no
    ).

% ---- 消す ----

del_assoc(Key, Assoc0, Value, Assoc) :-
    remove_node(Assoc0, Key, Value, Assoc, _).

% 最後の引数は、木が元より低くなったか。鍵が無ければ失敗する。
remove_node(Assoc0, Key, Value, Assoc, Shrunk) :-
    Assoc0 = t(Key0, Value0, Balance, Left, Right),
    compare(Order, Key, Key0),
    (   Order = (=)
    ->  Value = Value0,
        remove_root(Balance, Key0, Left, Right, Assoc, Shrunk)
    ;   Order = (<)
    ->  remove_node(Left, Key, Value, Left1, LeftShrunk),
        left_shrank(LeftShrunk, Balance, Key0, Value0, Left1, Right, Assoc, Shrunk)
    ;   remove_node(Right, Key, Value, Right1, RightShrunk),
        right_shrank(RightShrunk, Balance, Key0, Value0, Left, Right1, Assoc, Shrunk)
    ).

% 根を抜く。子が片方だけならその子が代わりになり、2 つあれば右の最小の節を根に持ち上げる。
remove_root(Balance, _, Left, Right, Assoc, Shrunk) :-
    (   Left == t
    ->  Assoc = Right,
        Shrunk = yes
    ;   Right == t
    ->  Assoc = Left,
        Shrunk = yes
    ;   take_min(Right, KeyMin, ValueMin, Right1, RightShrunk),
        right_shrank(RightShrunk, Balance, KeyMin, ValueMin, Left, Right1, Assoc, Shrunk)
    ).

del_min_assoc(Assoc0, Key, Value, Assoc) :-
    take_min(Assoc0, Key, Value, Assoc, _).

del_max_assoc(Assoc0, Key, Value, Assoc) :-
    take_max(Assoc0, Key, Value, Assoc, _).

take_min(t(Key0, Value0, Balance, Left0, Right), Key, Value, Assoc, Shrunk) :-
    (   Left0 == t
    ->  Key = Key0,
        Value = Value0,
        Assoc = Right,
        Shrunk = yes
    ;   take_min(Left0, Key, Value, Left, LeftShrunk),
        left_shrank(LeftShrunk, Balance, Key0, Value0, Left, Right, Assoc, Shrunk)
    ).

take_max(t(Key0, Value0, Balance, Left, Right0), Key, Value, Assoc, Shrunk) :-
    (   Right0 == t
    ->  Key = Key0,
        Value = Value0,
        Assoc = Left,
        Shrunk = yes
    ;   take_max(Right0, Key, Value, Right, RightShrunk),
        right_shrank(RightShrunk, Balance, Key0, Value0, Left, Right, Assoc, Shrunk)
    ).

% 左が 1 低くなったかもしれない。低くなっていなければ節はそのまま。
% 左が重かったなら釣り合い（全体も低くなる）、釣り合っていたなら右が重くなる（全体は同じ）、
% 右が重かったなら差が 2 になるので回す。
left_shrank(no, Balance, Key, Value, Left, Right, t(Key, Value, Balance, Left, Right), no).
left_shrank(yes, Balance, Key, Value, Left, Right, Assoc, Shrunk) :-
    (   Balance = (>)
    ->  Assoc = t(Key, Value, =, Left, Right),
        Shrunk = yes
    ;   Balance = (=)
    ->  Assoc = t(Key, Value, <, Left, Right),
        Shrunk = no
    ;   tilt_left(Key, Value, Left, Right, Assoc, Shrunk)
    ).

right_shrank(no, Balance, Key, Value, Left, Right, t(Key, Value, Balance, Left, Right), no).
right_shrank(yes, Balance, Key, Value, Left, Right, Assoc, Shrunk) :-
    (   Balance = (<)
    ->  Assoc = t(Key, Value, =, Left, Right),
        Shrunk = yes
    ;   Balance = (=)
    ->  Assoc = t(Key, Value, >, Left, Right),
        Shrunk = no
    ;   tilt_right(Key, Value, Left, Right, Assoc, Shrunk)
    ).

% ---- 並べる ----

% 右から先に辿って前に積めば、昇順のリストが 1 回の走査でできる。
assoc_to_list(Assoc, Pairs) :-
    collect_pairs(Assoc, [], Pairs).

collect_pairs(Assoc, Tail, Pairs) :-
    (   Assoc == t
    ->  Pairs = Tail
    ;   Assoc = t(Key, Value, _, Left, Right),
        collect_pairs(Right, Tail, Later),
        collect_pairs(Left, [Key-Value|Later], Pairs)
    ).

assoc_to_keys(Assoc, Keys) :-
    assoc_to_list(Assoc, Pairs),
    keys_of(Pairs, Keys).

assoc_to_values(Assoc, Values) :-
    assoc_to_list(Assoc, Pairs),
    values_of(Pairs, Values).

keys_of([], []).
keys_of([Key-_|Pairs], [Key|Keys]) :-
    keys_of(Pairs, Keys).

values_of([], []).
values_of([_-Value|Pairs], [Value|Values]) :-
    values_of(Pairs, Values).

% ---- 確かめる・写す ----

is_assoc(Assoc) :-
    nonvar(Assoc),
    (   Assoc == t
    ->  true
    ;   Assoc = t(_, _, _, _, _),
        checked_height(Assoc, none, none, _)
    ).

% 鍵が下限と上限の間で狭義に昇り、Balance が左右の高さの比べと合い、差が 1 以下であること。
checked_height(Assoc, Low, High, Height) :-
    nonvar(Assoc),
    (   Assoc == t
    ->  Height = 0
    ;   Assoc = t(Key, _, Balance, Left, Right),
        above_bound(Low, Key),
        below_bound(High, Key),
        checked_height(Left, Low, bound(Key), LeftHeight),
        checked_height(Right, bound(Key), High, RightHeight),
        compare(Order, LeftHeight, RightHeight),
        Order == Balance,
        abs(LeftHeight - RightHeight) =< 1,
        Height is max(LeftHeight, RightHeight) + 1
    ).

above_bound(none, _).
above_bound(bound(Low), Key) :- Low @< Key.

below_bound(none, _).
below_bound(bound(High), Key) :- Key @< High.

map_assoc(Goal, Assoc) :-
    (   Assoc == t
    ->  true
    ;   Assoc = t(_, Value, _, Left, Right),
        map_assoc(Goal, Left),
        call(Goal, Value),
        map_assoc(Goal, Right)
    ).

map_assoc(Goal, Assoc0, Assoc) :-
    (   Assoc0 == t
    ->  Assoc = t
    ;   Assoc0 = t(Key, Value0, Balance, Left0, Right0),
        Assoc = t(Key, Value, Balance, Left, Right),
        map_assoc(Goal, Left0, Left),
        call(Goal, Value0, Value),
        map_assoc(Goal, Right0, Right)
    ).
