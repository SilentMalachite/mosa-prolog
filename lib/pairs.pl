% library(pairs)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_pairs.pl。
% 名前と引数の順は SWI-Prolog のマニュアル（10.1.20、2026-11-16 に読んだ）の pairs の節に合わせ、コードは新しく書いた。
% マニュアルに誤りの記述は無いので、誤りは足さない。group_pairs_by_key/2 は隣り合って == の鍵だけをまとめる。
:- module(pairs, [pairs_keys_values/3, pairs_keys/2, pairs_values/2,
                  group_pairs_by_key/2, transpose_pairs/2, map_list_to_pairs/3]).

:- meta_predicate map_list_to_pairs(2, ?, ?).

% 初めの真のリストを先頭の引数にして絞る。真のリストで絞れば、どの向きでも選択点を残さない（レビュー L-34）。
% どれも真のリストでなければ、前と同じく組のリストが決まっていれば組を、そうでなければ鍵のリストを先頭にする。
pairs_keys_values(Pairs, Keys, Values) :-
    (   is_list(Pairs)
    ->  split_pairs(Pairs, Keys, Values)
    ;   is_list(Keys)
    ->  walk_triple(Keys, Values, Pairs)
    ;   is_list(Values)
    ->  walk_values(Values, Keys, Pairs)
    ;   nonvar(Pairs)
    ->  split_pairs(Pairs, Keys, Values)
    ;   walk_triple(Keys, Values, Pairs)
    ).

split_pairs([], [], []).
split_pairs([Key-Value|Pairs], [Key|Keys], [Value|Values]) :-
    split_pairs(Pairs, Keys, Values).

walk_triple([], [], []).
walk_triple([Key|Keys], [Value|Values], [Key-Value|Pairs]) :-
    walk_triple(Keys, Values, Pairs).

walk_values([], [], []).
walk_values([Value|Values], [Key|Keys], [Key-Value|Pairs]) :-
    walk_values(Values, Keys, Pairs).

pairs_keys(Pairs, Keys) :-
    pairs_keys_values(Pairs, Keys, _).

pairs_values(Pairs, Values) :-
    pairs_keys_values(Pairs, _, Values).

% 先頭の鍵と == で続く値を集め切ってから、結果の引数に束縛する。鍵が == でない限り別の組にする
% （1 と 1.0 は別）。並べ替えは呼び手の仕事。
group_pairs_by_key([], []).
group_pairs_by_key([Key-First|Tail], Groups) :-
    collect_run(Tail, Key, Others, Rest),
    group_pairs_by_key(Rest, Later),
    Groups = [Key-[First|Others]|Later].

collect_run([], _, [], []).
collect_run([Key-Value|Tail], Wanted, Others, Rest) :-
    (   Key == Wanted
    ->  Others = [Value|More],
        collect_run(Tail, Wanted, More, Rest)
    ;   Others = [],
        Rest = [Key-Value|Tail]
    ).

% 鍵と値を取り替えた組を同じ述語で作り、keysort/2 する。keysort/2 は安定なので、同じ鍵の中は元の順のまま。
transpose_pairs(Pairs, Transposed) :-
    pairs_keys_values(Pairs, Keys, Values),
    pairs_keys_values(Swapped, Values, Keys),
    keysort(Swapped, Transposed).

% 鍵を先に全部求め、リストと組にする。
map_list_to_pairs(Function, List, Pairs) :-
    derive_keys(List, Function, Keys),
    pairs_keys_values(Pairs, Keys, List).

derive_keys([], _, []).
derive_keys([Item|Items], Function, [Key|Keys]) :-
    call(Function, Item, Key),
    derive_keys(Items, Function, Keys).
