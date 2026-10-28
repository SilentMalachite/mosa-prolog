% library(strings)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_strings.pl。
% 誤りの形は Logtalk の atomic_list_concat_2、atomic_list_concat_3 と同じ。結果の引数は atom_concat/3（8.16.2）に
% 合わせて変数か原子に限る。split_atom/4 は SWI の split_string/4 と同じ切り方で、結果は原子のリスト。
:- module(strings, [atomic_list_concat/2, atomic_list_concat/3, split_atom/4]).
:- use_module(library(lists)).


atomic_list_concat(List, Atom) :-
    must_be_list(List, atomic_list_concat/2),
    items_chars(List, '', atomic_list_concat/2, Chars),
    must_be_atom_or_var(Atom, atomic_list_concat/2),
    atom_chars(Atom, Chars).

% リストが全部決まっていれば繋ぎ、そうでなく原子が決まっていれば分ける。
atomic_list_concat(List, Sep, Atom) :-
    PI = atomic_list_concat/3,
    must_be_atomic(Sep, PI),
    must_be_atom_or_var(Atom, PI),
    (   joinable(List)
    ->  items_chars(List, Sep, PI, Chars),
        atom_chars(Atom, Chars)
    ;   atom(Atom)
    ->  must_be_list_or_partial(List, PI),
        split_by(Sep, Atom, PI, List)
    ;   must_be_list(List, PI),
        items_chars(List, Sep, PI, _)
    ).

% 繋げるのは真のリストで、要素がどれも変数でないとき。要素の型はあとで見る。
joinable(List) :-
    is_list(List),
    \+ (member(X, List), var(X)).

split_by(Sep, Atom, PI, List) :-
    (   Sep == ''
    ->  throw(error(domain_error(non_empty_atom, Sep), context(PI, _)))
    ;   true
    ),
    atomic_chars(Sep, SepChars),
    atom_chars(Atom, Chars),
    split_chars(Chars, SepChars, Parts),
    chars_atoms(Parts, List).

% 最初に現れる区切りで切る。append/3 は短い前半から試すので、最初の位置が先に見つかる。
split_chars(Chars, Sep, [Part|Parts]) :-
    (   append(Part0, Rest0, Chars),
        append(Sep, Rest, Rest0)
    ->  Part = Part0,
        split_chars(Rest, Sep, Parts)
    ;   Part = Chars,
        Parts = []
    ).

items_chars([], _, _, []).
items_chars([X|Xs], Sep, PI, Chars) :-
    must_be_atomic(X, PI),
    atomic_chars(X, Cs),
    (   Xs == []
    ->  Chars = Cs
    ;   atomic_chars(Sep, SepChars),
        append(SepChars, Rest, Tail),
        append(Cs, Tail, Chars),
        items_chars(Xs, Sep, PI, Rest)
    ).

% split_atom(+Text, +SepChars, +PadChars, -Atoms)。先に全体の末尾の詰め文字を除く。
split_atom(Text, SepText, PadText, Atoms) :-
    PI = split_atom/4,
    must_be_atomic(Text, PI),
    must_be_atomic(SepText, PI),
    must_be_atomic(PadText, PI),
    atomic_chars(Text, Chars0),
    atomic_chars(SepText, Sep),
    atomic_chars(PadText, Pad),
    reverse(Chars0, Rev0),
    drop_pad(Rev0, Pad, Rev),
    reverse(Rev, Chars),
    skip_and_split(Chars, Sep, Pad, Parts),
    chars_atoms(Parts, Atoms).

skip_and_split(Chars0, Sep, Pad, Parts) :-
    drop_pad(Chars0, Pad, Chars),
    (   Chars == []
    ->  Parts = [[]]
    ;   split_fields(Chars, Sep, Pad, Parts)
    ).

% 区切りが詰め文字でなく、すぐ後ろも区切りなら、詰め文字を飛ばさずに空の部分を作る（SWI と同じ）。
split_fields(Chars, Sep, Pad, [Part|Parts]) :-
    take_field(Chars, Sep, Field, Rest),
    reverse(Field, RevField0),
    drop_pad(RevField0, Pad, RevField),
    reverse(RevField, Part),
    (   Rest = [S|Rest1]
    ->  (   \+ memberchk(S, Pad),
            Rest1 = [Next|_],
            memberchk(Next, Sep)
        ->  split_fields(Rest1, Sep, Pad, Parts)
        ;   skip_and_split(Rest1, Sep, Pad, Parts)
        )
    ;   Parts = []
    ).

take_field([], _, [], []).
take_field([C|Cs], Sep, Field, Rest) :-
    (   memberchk(C, Sep)
    ->  Field = [],
        Rest = [C|Cs]
    ;   Field = [C|Field1],
        take_field(Cs, Sep, Field1, Rest)
    ).

drop_pad([C|Cs], Pad, Rest) :-
    memberchk(C, Pad),
    !,
    drop_pad(Cs, Pad, Rest).
drop_pad(Cs, _, Cs).

chars_atoms([], []).
chars_atoms([Cs|Css], [A|As]) :-
    atom_chars(A, Cs),
    chars_atoms(Css, As).

atomic_chars(X, Chars) :-
    (   atom(X)
    ->  atom_chars(X, Chars)
    ;   number_chars(X, Chars)
    ).

must_be_atomic(X, PI) :-
    (   var(X)
    ->  throw(error(instantiation_error, context(PI, _)))
    ;   atomic(X)
    ->  true
    ;   throw(error(type_error(atomic, X), context(PI, _)))
    ).

must_be_atom_or_var(X, PI) :-
    (   var(X)
    ->  true
    ;   atom(X)
    ->  true
    ;   throw(error(type_error(atom, X), context(PI, _)))
    ).

% 部分リストは具体化の誤り、それ以外の形は型の誤り。
must_be_list(List, PI) :-
    must_be_list_or_partial(List, PI),
    list_tail(List, Tail),
    (   Tail == []
    ->  true
    ;   throw(error(instantiation_error, context(PI, _)))
    ).

must_be_list_or_partial(List, PI) :-
    list_tail(List, Tail),
    (   var(Tail)
    ->  true
    ;   Tail == []
    ->  true
    ;   throw(error(type_error(list, List), context(PI, _)))
    ).

list_tail(List, Tail) :-
    (   nonvar(List),
        List = [_|Rest]
    ->  list_tail(Rest, Tail)
    ;   Tail = List
    ).
