% library(japanese)（SPEC 第5節のライブラリ表）。テストは tests/pl/japanese.pl。
% ひらがな_カタカナ/2 と 全角_半角/2 は、第1引数が決まっていれば前向き、第2引数だけ決まっていれば逆向きに写す。
% 前向きの写像は 1 対 1 でない（さ と サ はどちらも サ になる）ので、逆向きは解を 1 つに決めて逆の写像を使う。
% 入力は原子か数、結果は変数か原子で、library(strings) の upcase_atom/2 と同じ誤りを出す。
:- module(japanese, [ひらがな_カタカナ/2, 全角_半角/2, 読み幅/2]).


ひらがな_カタカナ(Hiragana, Katakana) :-
    convert(Hiragana, Katakana, ひらがな_カタカナ/2, to_katakana, to_hiragana).

全角_半角(Full, Half) :-
    convert(Full, Half, 全角_半角/2, to_halfwidth, to_fullwidth).

読み幅(Text, Width) :-
    must_be_atomic(Text, 読み幅/2),
    (   var(Width)
    ->  true
    ;   integer(Width)
    ->  true
    ;   throw(error(type_error(integer, Width), context(読み幅/2, _)))
    ),
    atomic_codes(Text, Codes),
    sum_widths(Codes, 0, Width).

sum_widths([], Width, Width).
sum_widths([C|Cs], Width0, Width) :-
    '$char_width'(C, W),
    Width1 is Width0 + W,
    sum_widths(Cs, Width1, Width).

convert(From, To, PI, Forward, Backward) :-
    (   nonvar(From)
    ->  must_be_atomic(From, PI),
        must_be_atom_or_var(To, PI),
        map_text(Forward, From, To)
    ;   nonvar(To)
    ->  must_be_atomic(To, PI),
        map_text(Backward, To, From)
    ;   throw(error(instantiation_error, context(PI, _)))
    ).

map_text(Map, Text, Result) :-
    atomic_codes(Text, Codes0),
    map_codes(Codes0, Map, Codes),
    atom_codes(Result, Codes).

map_codes([], _, []).
map_codes([C0|Cs0], Map, [C|Cs]) :-
    map_code(Map, C0, C),
    map_codes(Cs0, Map, Cs).

% ひらがなは U+3041〜U+3096、カタカナは U+30A1〜U+30F6 で、0x60 ずつ離れている。
% 全角は U+FF01〜U+FF5E と全角空白 U+3000、半角は U+0021〜U+007E と空白。半角カタカナは扱わない。
map_code(to_katakana, C0, C) :-
    shift(C0, 0x3041, 0x3096, 0x60, C).
map_code(to_hiragana, C0, C) :-
    shift(C0, 0x30A1, 0x30F6, -0x60, C).
map_code(to_halfwidth, C0, C) :-
    (   C0 =:= 0x3000
    ->  C = 0x20
    ;   shift(C0, 0xFF01, 0xFF5E, -0xFEE0, C)
    ).
map_code(to_fullwidth, C0, C) :-
    (   C0 =:= 0x20
    ->  C = 0x3000
    ;   shift(C0, 0x21, 0x7E, 0xFEE0, C)
    ).

shift(C0, Low, High, Offset, C) :-
    (   C0 >= Low,
        C0 =< High
    ->  C is C0 + Offset
    ;   C = C0
    ).

atomic_codes(X, Codes) :-
    (   atom(X)
    ->  atom_codes(X, Codes)
    ;   number_codes(X, Codes)
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
