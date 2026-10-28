:- module(strings, [
    atomic_list_concat/2,
    atomic_list_concat/3,
    split_atom/4,
    upcase_atom/2
]).

% 区切りなしは空アトムを挟むのと同じ。
atomic_list_concat(List, Atom) :-
    atomic_list_concat(List, '', Atom).

% リストが変数なら、区切りアトムで分ける。具体値なら繋ぐ。
atomic_list_concat(List, Sep, Atom) :-
    var(List), !,
    split_atomic(Sep, Atom, List).
atomic_list_concat(List, Sep, Atom) :-
    join_list(List, Sep, Atom).

% 末尾から繋ぐ。開いた末尾は分け始めない。空の結果には区切りを足さない。
join_list(List, _, _) :-
    var(List), !,
    throw(error(instantiation_error, _)).
join_list([], Sep, '') :-
    separator_text(Sep, _).
join_list([Head|Tail], Sep, Atom) :-
    piece_atom(Head, Text),
    join_list(Tail, Sep, Rest),
    join_piece(Text, Sep, Rest, Atom).
join_list(Other, _, _) :-
    throw(error(type_error(list, Other), _)).

% 数は number_codes の字面にする。アトムはそのまま。変数は型より先。
piece_atom(Term, _) :-
    var(Term), !,
    throw(error(instantiation_error, _)).
piece_atom(Term, Term) :- atom(Term), !.
piece_atom(Term, Atom) :-
    number(Term), !,
    number_codes(Term, Codes),
    atom_codes(Atom, Codes).
piece_atom(Term, _) :-
    throw(error(type_error(atomic, Term), _)).

join_piece(Head, Sep, '', Head) :-
    separator_text(Sep, _).
join_piece(Head, Sep, Rest, Atom) :-
    Rest \== '',
    separator_text(Sep, SepText),
    atom_concat(SepText, Rest, Joined),
    atom_concat(Head, Joined, Atom).

% 区切りはアトミック。数は字面にする。変数は型より先。
separator_text(Sep, _) :-
    var(Sep), !,
    throw(error(instantiation_error, _)).
separator_text(Sep, Sep) :- atom(Sep), !.
separator_text(Sep, Text) :-
    number(Sep), !,
    number_codes(Sep, Codes),
    atom_codes(Text, Codes).
separator_text(Sep, _) :-
    throw(error(type_error(atomic, Sep), _)).

% 区切りは1文字の集合ではなく、アトム全体を単位にする。空では分けない。
split_atomic(Sep, _, _) :-
    Sep == '', !,
    throw(error(domain_error(non_empty_atom, ''), _)).
split_atomic(Sep, Atom, List) :-
    atom_codes(Sep, SepCodes),
    atom_codes(Atom, Codes),
    split_codes(Codes, SepCodes, Parts),
    parts_atoms(Parts, List).

split_codes(Codes, Sep, [Part|Parts]) :-
    take_until(Codes, Sep, Part, After), !,
    split_codes(After, Sep, Parts).
split_codes(Codes, _, [Codes]).

take_until(Codes, Sep, [], After) :-
    starts_with(Sep, Codes, After), !.
take_until([Code|Codes], Sep, [Code|Part], After) :-
    take_until(Codes, Sep, Part, After).

starts_with([], Codes, Codes).
starts_with([Code|Sep], [Code|Codes], After) :-
    starts_with(Sep, Codes, After).

parts_atoms([], []).
parts_atoms([Codes|Rest], [Atom|Atoms]) :-
    atom_codes(Atom, Codes),
    parts_atoms(Rest, Atoms).

% 区切りアトムの各文字で分ける。埋め文字は各部分の前後から捨てる。
split_atom(Atom, Seps, Pad, Parts) :-
    atom_codes(Atom, Codes),
    atom_codes(Seps, SepCodes),
    atom_codes(Pad, PadCodes),
    split_fields(Codes, SepCodes, PadCodes, PartCodes),
    parts_atoms(PartCodes, Parts).

% 全体が埋めだけなら部分は無い。空の入力は空アトム一つ。
split_fields(Codes, Sep, Pad, Parts) :-
    skip_set(Codes, Pad, Rest),
    (   Rest == [], Codes \== [] ->
        Parts = []
    ;   Rest == [] ->
        Parts = [[]]
    ;   next_fields(Rest, Sep, Pad, Parts)
    ).

next_fields(Codes, Sep, Pad, [Part|Parts]) :-
    field_mark(Codes, Sep, Raw, Mark),
    trim_end(Raw, Pad, Part),
    rest_fields(Mark, Sep, Pad, Parts).

rest_fields(end, _, _, []).
rest_fields(sep(SepCode, After), Sep, Pad, Parts) :-
    after_sep(SepCode, After, Sep, Pad, Rest),
    (   Rest == [] ->
        end_after_sep(SepCode, Pad, Parts)
    ;   next_fields(Rest, Sep, Pad, Parts)
    ).

% 末尾の区切りが埋めなら空の部分は残さない。
end_after_sep(SepCode, Pad, []) :-
    code_in(SepCode, Pad), !.
end_after_sep(_, _, [[]]).

field_mark([], _, [], end).
field_mark([Code|Codes], Sep, [], sep(Code, Codes)) :-
    code_in(Code, Sep), !.
field_mark([Code|Codes], Sep, [Code|Part], Mark) :-
    field_mark(Codes, Sep, Part, Mark).

% 区切り自身が埋めなら、続く埋めも区切りとして畳む。
after_sep(SepCode, After, _, Pad, Rest) :-
    code_in(SepCode, Pad), !,
    skip_set(After, Pad, Rest).
after_sep(_, After, Sep, Pad, Rest) :-
    skip_pad_keep_sep(After, Sep, Pad, Rest).

skip_set([], _, []).
skip_set([Code|Codes], Set, Rest) :-
    code_in(Code, Set), !,
    skip_set(Codes, Set, Rest).
skip_set(Codes, _, Codes).

skip_pad_keep_sep([], _, _, []).
skip_pad_keep_sep([Code|Codes], Sep, Pad, Rest) :-
    code_in(Code, Pad),
    \+ code_in(Code, Sep), !,
    skip_pad_keep_sep(Codes, Sep, Pad, Rest).
skip_pad_keep_sep(Codes, _, _, Codes).

trim_end(Codes, Pad, Trimmed) :-
    reverse_codes(Codes, Rev),
    skip_set(Rev, Pad, RevTrim),
    reverse_codes(RevTrim, Trimmed).

reverse_codes(Codes, Rev) :- reverse_codes(Codes, [], Rev).
reverse_codes([], Acc, Acc).
reverse_codes([Code|Codes], Acc, Rev) :- reverse_codes(Codes, [Code|Acc], Rev).

code_in(Code, [Code|_]).
code_in(Code, [_|Rest]) :- code_in(Code, Rest).

% 単純大文字写像は system の表。修飾なしでは user から見えない。
upcase_atom(Atom, Upper) :-
    system:upcase_atom(Atom, Upper).
