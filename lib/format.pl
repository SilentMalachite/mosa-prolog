% library(format)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_format.pl。
% 指示は ~w ~q ~a ~d ~f ~e ~s ~c ~n ~t ~| ~~ だけ。誤りは iso-notes の Section 5 format/2 errors の決定 A。
% 出力を全部組んでから一度に書く。途中で誤りが出ても、半端な出力を残さない。
:- module(format, [format/1, format/2, format/3]).
:- use_module(library(lists)).


format(Format) :-
    format_atom(Format, [], format/1, Text),
    write(Text).

format(Format, Args) :-
    format_atom(Format, Args, format/2, Text),
    write(Text).

format(Stream, Format, Args) :-
    format_atom(Format, Args, format/3, Text),
    write(Stream, Text).

format_atom(Format, Args, PI, Text) :-
    text_chars(Format, PI, Chars),
    must_be_list(Args, PI),
    directives(Chars, Args, PI, st(0, [], []), Out),
    atom_chars(Text, Out).

% 書式と ~s の引数は、原子、文字のリスト、文字コードのリストのどれでもよい。
% "" は [] であり原子でもあるので、原子より先に空のリストとして見る。
text_chars(Text, PI, _) :-
    var(Text),
    throw(error(instantiation_error, context(PI, _))).
text_chars([], _, []) :-
    !.
text_chars(Text, _, Chars) :-
    atom(Text),
    !,
    atom_chars(Text, Chars).
text_chars(Text, PI, Chars) :-
    must_be_list(Text, PI),
    text_items(Text, PI, Chars).

text_items([], _, []).
text_items([X|Xs], PI, [C|Cs]) :-
    text_item(X, PI, C),
    text_items(Xs, PI, Cs).

text_item(X, PI, _) :-
    var(X),
    throw(error(instantiation_error, context(PI, _))).
text_item(X, _, C) :-
    integer(X),
    !,
    char_code(C, X).
text_item(X, _, X) :-
    atom(X),
    atom_length(X, 1),
    !.
text_item(X, PI, _) :-
    throw(error(type_error(character, X), context(PI, _))).

% 部分リストは具体化の誤り、それ以外の形は型の誤り。
must_be_list(List, _) :-
    is_list(List),
    !.
must_be_list(List, PI) :-
    list_tail(List, Tail),
    (   var(Tail)
    ->  throw(error(instantiation_error, context(PI, _)))
    ;   throw(error(type_error(list, List), context(PI, _)))
    ).

list_tail(List, Tail) :-
    (   nonvar(List),
        List = [_|Rest]
    ->  list_tail(Rest, Tail)
    ;   Tail = List
    ).

directives([], Args, PI, St, Out) :-
    (   Args == []
    ->  true
    ;   throw(error(domain_error(empty_list, Args), context(PI, _)))
    ),
    flush(St, st(_, _, Rev)),
    reverse(Rev, Out).
directives(['~'|Cs0], Args0, PI, St0, Out) :-
    !,
    count(Cs0, Args0, PI, Cs1, Args1, Count),
    (   Cs1 = [D|Cs]
    ->  directive(D, Count, Args1, Args, PI, St0, St)
    ;   throw(error(domain_error(format_directive, '~'), context(PI, _)))
    ),
    directives(Cs, Args, PI, St, Out).
directives([C|Cs], Args, PI, St0, Out) :-
    emit([C], St0, St),
    directives(Cs, Args, PI, St, Out).

% ~N、~*、~`c の 3 つの書き方。無ければ none。
count(['*'|Cs], Args0, PI, Cs, Args, N) :-
    !,
    next_arg(Args0, PI, X, Args),
    evaluate_integer(X, PI, N),
    not_less_than_zero(N, PI).
count(['`', C|Cs], Args, _, Cs, Args, N) :-
    !,
    char_code(C, N).
count([D|Cs0], Args, _, Cs, Args, N) :-
    digit(D, N0),
    !,
    digits(Cs0, N0, Cs, N).
count(Cs, Args, _, Cs, Args, none).

digits([D|Cs0], N0, Cs, N) :-
    digit(D, V),
    !,
    N1 is N0 * 10 + V,
    digits(Cs0, N1, Cs, N).
digits(Cs, N, Cs, N).

digit(D, V) :-
    char_code(D, Code),
    Code >= 0'0,
    Code =< 0'9,
    V is Code - 0'0.

next_arg([], PI, _, _) :-
    throw(error(domain_error(non_empty_list, []), context(PI, _))).
next_arg([X|Xs], _, X, Xs).

% 数の引数は is/2 と同じく評価するので、その誤りは is/2 のものになる。
evaluate_integer(X, PI, N) :-
    N is X,
    (   integer(N)
    ->  true
    ;   throw(error(type_error(integer, N), context(PI, _)))
    ).

not_less_than_zero(N, PI) :-
    (   N < 0
    ->  throw(error(domain_error(not_less_than_zero, N), context(PI, _)))
    ;   true
    ).

default(none, N, N) :- !.
default(N, _, N).

directive(w, _, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    with_output_to(atom(A), write(X)),
    emit_atom(A, St0, St).
directive(q, _, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    with_output_to(atom(A), writeq(X)),
    emit_atom(A, St0, St).
directive(a, _, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    (   var(X)
    ->  throw(error(instantiation_error, context(PI, _)))
    ;   atom(X)
    ->  emit_atom(X, St0, St)
    ;   throw(error(type_error(atom, X), context(PI, _)))
    ).
directive(d, Count, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    evaluate_integer(X, PI, N),
    default(Count, 0, Places),
    decimal_chars(N, Places, Chars),
    emit(Chars, St0, St).
directive(f, Count, Args0, Args, PI, St0, St) :-
    !,
    float_directive(f, Count, Args0, Args, PI, St0, St).
directive(e, Count, Args0, Args, PI, St0, St) :-
    !,
    float_directive(e, Count, Args0, Args, PI, St0, St).
directive(s, Count, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    text_chars(X, PI, Chars0),
    (   Count == none
    ->  Chars = Chars0
    ;   take_padded(Count, Chars0, Chars)
    ),
    emit(Chars, St0, St).
directive(c, Count, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    evaluate_integer(X, PI, Code),
    char_code(C, Code),
    default(Count, 1, N),
    repeat_char(N, C, Chars),
    emit(Chars, St0, St).
directive(n, Count, Args, Args, _, St0, St) :-
    !,
    default(Count, 1, N),
    repeat_char(N, '\n', Chars),
    emit(Chars, St0, St1),
    flush(St1, St).
directive('~', _, Args, Args, _, St0, St) :-
    !,
    emit(['~'], St0, St).
directive(t, Count, Args, Args, _, st(Col, Seg, Out), st(Col, [fill(C)|Seg], Out)) :-
    !,
    (   Count == none
    ->  C = ' '
    ;   char_code(C, Count)
    ).
directive('|', Count, Args, Args, _, St0, St) :-
    !,
    column_stop(Count, St0, St).
directive(D, _, _, _, PI, _, _) :-
    throw(error(domain_error(format_directive, D), context(PI, _))).

float_directive(Kind, Count, Args0, Args, PI, St0, St) :-
    next_arg(Args0, PI, X, Args),
    V is X,
    F is float(V),
    default(Count, 6, Digits),
    '$format_float'(Kind, Digits, F, A),
    emit_atom(A, St0, St).

% ~Nd は右から N 桁目の前に小数点を置く。桁が足りなければ 0 で埋める。
decimal_chars(N, 0, Chars) :-
    !,
    number_chars(N, Chars).
decimal_chars(N, Places, Chars) :-
    Abs is abs(N),
    number_chars(Abs, Digits0),
    length(Digits0, Len),
    Zeros is max(0, Places + 1 - Len),
    repeat_char(Zeros, '0', Pad),
    append(Pad, Digits0, Digits),
    length(Frac, Places),
    append(Int, Frac, Digits),
    append(Int, ['.'|Frac], Body),
    (   N < 0
    ->  Chars = ['-'|Body]
    ;   Chars = Body
    ).

take_padded(0, _, []) :- !.
take_padded(N, [C|Cs], [C|Rest]) :-
    !,
    N1 is N - 1,
    take_padded(N1, Cs, Rest).
take_padded(N, [], Pad) :-
    repeat_char(N, ' ', Pad).

repeat_char(0, _, []) :- !.
repeat_char(N, C, [C|Cs]) :-
    N1 is N - 1,
    repeat_char(N1, C, Cs).

% 状態は st(区切りの始まりの桁, 区切りの中の項目の逆順, 確定した出力の逆順)。
% 項目は text(文字のリスト) か fill(埋める文字)。~| と ~n で区切りを確定する。
emit_atom(A, St0, St) :-
    atom_chars(A, Chars),
    emit(Chars, St0, St).

emit(Chars, st(Col, Seg, Out), st(Col, [text(Chars)|Seg], Out)).

flush(st(Col, Seg, Out), St) :-
    reverse(Seg, Items),
    place(Items, 0, Col, Out, St).

% 桁は東アジア幅で数える。幅の表は 読み幅/2 と同じ '$char_width'/2（src/unicode_tables.inc）。
column_stop(Count, st(Col, Seg, Out), St) :-
    reverse(Seg, Items0),
    items_text(Items0, Text),
    column_after(Text, Col, Here),
    default(Count, Here, Target),
    Pad is max(0, Target - Here),
    (   member(fill(_), Items0)
    ->  Items = Items0
    ;   append(Items0, [fill(' ')], Items)
    ),
    place(Items, Pad, Col, Out, St).

% 余りは後ろの埋め所から 1 つずつ足す（Logtalk の pip_0110 の例と同じ）。
place(Items, Pad, Col, Out0, st(Col1, [], Out)) :-
    count_fills(Items, 0, Fills),
    (   Fills =:= 0
    ->  Base = 0,
        Extra = 0
    ;   Base is Pad // Fills,
        Extra is Pad mod Fills
    ),
    First is Fills - Extra,
    expand(Items, 1, First, Base, Chars),
    column_after(Chars, Col, Col1),
    reverse(Chars, Rev),
    append(Rev, Out0, Out).

count_fills([], N, N).
count_fills([Item|Items], N0, N) :-
    (   Item = fill(_)
    ->  N1 is N0 + 1
    ;   N1 = N0
    ),
    count_fills(Items, N1, N).

expand([], _, _, _, []).
expand([text(Text)|Items], I, First, Base, Chars) :-
    append(Text, Rest, Chars),
    expand(Items, I, First, Base, Rest).
expand([fill(C)|Items], I, First, Base, Chars) :-
    (   I > First
    ->  N is Base + 1
    ;   N = Base
    ),
    fill_chars(N, C, Pad),
    append(Pad, Rest, Chars),
    I1 is I + 1,
    expand(Items, I1, First, Base, Rest).

% N は埋める桁の数。字の幅で割り、余りの桁は空白で埋めて桁位置をそろえる（iso-notes の決定 A）。
% 幅 0 の字は 1 として数え、埋め草が尽きないようにする。
fill_chars(N, C, Chars) :-
    char_code(C, Code),
    '$char_width'(Code, W0),
    W is max(1, W0),
    Count is N // W,
    Rest is N mod W,
    repeat_char(Count, C, Fill),
    repeat_char(Rest, ' ', Spaces),
    append(Fill, Spaces, Chars).

items_text([], []).
items_text([text(Text)|Items], Chars) :-
    !,
    append(Text, Rest, Chars),
    items_text(Items, Rest).
items_text([_|Items], Chars) :-
    items_text(Items, Chars).

column_after([], Col, Col).
column_after([C|Cs], Col0, Col) :-
    (   C == '\n'
    ->  Col1 = 0
    ;   char_code(C, Code),
        '$char_width'(Code, W),
        Col1 is Col0 + W
    ),
    column_after(Cs, Col1, Col).
