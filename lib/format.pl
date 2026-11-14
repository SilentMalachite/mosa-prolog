% library(format)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_format.pl と、選択点を残さないことを見る
% tests/cpp/format_det_test.cpp。
% 指示は ~w ~p ~q ~k ~W ~a ~d ~D ~r ~R ~f ~e ~E ~g ~G ~s ~c ~i ~n ~N ~t ~| ~+ ~~ だけ。誤りは iso-notes の Section 5
% format/2 errors の決定 A。出力を全部組んでから一度に書く。途中で誤りが出ても、半端な出力を残さない。
% ~N だけは出力先の流れを見る（format/1,2 は呼んだときの現在出力、format/3 は引数の流れ）。~t、~|、~+ の桁は
% 今までと同じく呼び出しの始まりを 0 とする（フェーズ11の設計の非目標）。
:- module(format, [format/1, format/2, format/3]).
:- use_module(library(lists)).


format(Format) :-
    current_output(Stream),
    format_atom(Format, [], Stream, format/1, Text),
    write(Text).

format(Format, Args) :-
    current_output(Stream),
    format_atom(Format, Args, Stream, format/2, Text),
    write(Text).

format(Stream, Format, Args) :-
    format_atom(Format, Args, Stream, format/3, Text),
    write(Stream, Text).

format_atom(Format, Args, Stream, PI, Text) :-
    text_chars(Format, PI, Chars),
    must_be_list(Args, PI),
    directives(Chars, Args, Stream, PI, st(0, [], []), Out),
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

% 部分リストは具体化の誤り、それ以外の形は型の誤り。背骨が循環するリストを Prolog で辿ると止まらないので
% （レビュー L21）、形は length/2 に見させる。length/2 は巡回を type_error(list, List) にし、部分リストなら
% 末尾を束縛して成功する。その束縛は、続けて投げる具体化の誤りが戻す。\+ \+ で戻さないのは、\+ が実行時に
% ゴールを訳し直してリストを写し、共有の多い要素を木に広げるため（L21 の Codex 敵対的レビュー）。
must_be_list(List, _) :-
    is_list(List),
    !.
must_be_list(List, PI) :-
    catch(length(List, _), error(Formal, _), throw(error(Formal, context(PI, _)))),
    throw(error(instantiation_error, context(PI, _))).

% ~N だけが流れを要るので、directive/7 に流れを通さず、ここで分ける。
directives([], Args, _, PI, St, Out) :-
    (   Args == []
    ->  true
    ;   throw(error(domain_error(empty_list, Args), context(PI, _)))
    ),
    flush(St, st(_, _, Rev)),
    reverse(Rev, Out).
directives(['~'|Cs0], Args0, Stream, PI, St0, Out) :-
    !,
    count(Cs0, Args0, PI, Cs1, Args1, Count),
    (   Cs1 = ['N'|Cs]
    ->  Args = Args1,
        fresh_line(Stream, PI, St0, St)
    ;   Cs1 = [D|Cs]
    ->  directive(D, Count, Args1, Args, PI, St0, St)
    ;   throw(error(domain_error(format_directive, '~'), context(PI, _)))
    ),
    directives(Cs, Args, Stream, PI, St, Out).
directives([C|Cs], Args, Stream, PI, St0, Out) :-
    emit([C], St0, St),
    directives(Cs, Args, Stream, PI, St, Out).

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
directive(k, _, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    with_output_to(atom(A), write_canonical(X)),
    emit_atom(A, St0, St).
% 選択肢は write_term/2 が調べるので、その誤りは write_term/2 のものになる（~d の誤りが is/2 のものであるのと同じ）。
directive('W', _, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args1),
    next_arg(Args1, PI, Options, Args),
    with_output_to(atom(A), write_term(X, Options)),
    emit_atom(A, St0, St).
% print/1 は利用者の portray/1 を呼ぶ（iso-notes 8.14 の決定）。portray が現在出力へ書いた字も A に入る。
directive(p, _, Args0, Args, PI, St0, St) :-
    !,
    next_arg(Args0, PI, X, Args),
    with_output_to(atom(A), print(X)),
    emit_atom(A, St0, St).
% ~i は引数を 1 つ読み飛ばす。桁の数は使わない。
directive(i, _, Args0, Args, PI, St, St) :-
    !,
    next_arg(Args0, PI, _, Args).
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
    decimal_directive(plain, Count, Args0, Args, PI, St0, St).
directive('D', Count, Args0, Args, PI, St0, St) :-
    !,
    decimal_directive(grouped, Count, Args0, Args, PI, St0, St).
directive(r, Count, Args0, Args, PI, St0, St) :-
    !,
    radix_directive(lower, Count, Args0, Args, PI, St0, St).
directive('R', Count, Args0, Args, PI, St0, St) :-
    !,
    radix_directive(upper, Count, Args0, Args, PI, St0, St).
directive(f, Count, Args0, Args, PI, St0, St) :-
    !,
    float_directive(f, Count, Args0, Args, PI, St0, St).
directive(e, Count, Args0, Args, PI, St0, St) :-
    !,
    float_directive(e, Count, Args0, Args, PI, St0, St).
directive('E', Count, Args0, Args, PI, St0, St) :-
    !,
    float_directive('E', Count, Args0, Args, PI, St0, St).
directive(g, Count, Args0, Args, PI, St0, St) :-
    !,
    float_directive(g, Count, Args0, Args, PI, St0, St).
directive('G', Count, Args0, Args, PI, St0, St) :-
    !,
    float_directive('G', Count, Args0, Args, PI, St0, St).
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
    column_stop(absolute, Count, St0, St).
directive('+', Count, Args, Args, _, St0, St) :-
    !,
    column_stop(relative, Count, St0, St).
directive(D, _, _, _, PI, _, _) :-
    throw(error(domain_error(format_directive, D), context(PI, _))).

% ~N は、行の頭にいれば何も出さず、そうでなければ ~n と同じく改行を出して区切りを確定する。桁の数は使わない。
% 何も出さないときは区切りを確定しない。確定すると、前の ~t が後の ~| で埋めるはずの桁を失う。
fresh_line(Stream, PI, St0, St) :-
    (   line_start(St0, Stream, PI)
    ->  St = St0
    ;   emit(['\n'], St0, St1),
        flush(St1, St)
    ).

% 組んだ出力の最後の字が改行なら行の頭。何も組んでいなければ、流れの始まりの桁が 0 かで決める。流れの桁は
% 何も組んでいないときにだけ読むので、~N の無い書式で流れの誤りが書式の誤りより先に出ることは無い。
line_start(st(_, Seg, Out), Stream, PI) :-
    (   last_text_char(Seg, C)
    ->  C == '\n'
    ;   Out = [C|_]
    ->  C == '\n'
    ;   stream_column(Stream, PI, Col),
        Col =:= 0
    ).

% 区切りの項目は新しいものが先にある。埋め草と空の text は字を出していないので飛ばす。
last_text_char([text(Chars)|_], C) :-
    last(Chars, C),
    !.
last_text_char([_|Items], C) :-
    last_text_char(Items, C).

% 流れの誤りの文脈は '$stream_column'/2 でなく format の述語指示子にする。
stream_column(Stream, PI, Col) :-
    catch('$stream_column'(Stream, Col), error(Formal, _), throw(error(Formal, context(PI, _)))).

float_directive(Kind, Count, Args0, Args, PI, St0, St) :-
    next_arg(Args0, PI, X, Args),
    V is X,
    F is float(V),
    default(Count, 6, Digits),
    '$format_float'(Kind, Digits, F, A),
    emit_atom(A, St0, St).

% ~d と ~D は桁の数を小数部の桁の数にする。~D は整数部だけを区切るので、符号と整数部と小数部を分けて組む。
decimal_directive(Group, Count, Args0, Args, PI, St0, St) :-
    next_arg(Args0, PI, X, Args),
    evaluate_integer(X, PI, N),
    default(Count, 0, Places),
    decimal_parts(N, Places, Int0, Frac),
    group_digits(Group, Int0, Int),
    append(Int, Frac, Digits),
    (   N < 0
    ->  Chars = ['-'|Digits]
    ;   Chars = Digits
    ),
    emit(Chars, St0, St).

% ~Nd は右から N 桁目の前に小数点を置く。桁が足りなければ 0 で埋める。Int は整数部の数字、Frac は小数点からの字
% （N が 0 なら []）。最小の整数の abs はあふれるので、符号は字から外す（レビュー L7）。
decimal_parts(N, Places, Int, Frac) :-
    number_chars(N, Chars),
    (   Chars = ['-'|Digits0]
    ->  true
    ;   Digits0 = Chars
    ),
    (   Places =:= 0
    ->  Int = Digits0,
        Frac = []
    ;   length(Digits0, Len),
        Zeros is max(0, Places + 1 - Len),
        repeat_char(Zeros, '0', Pad),
        append(Pad, Digits0, Digits),
        % 整数部の長さを先に決める。Int が未束縛のまま append/3 で分けると選択点が残る（0.1.1）。
        IntLen is Len + Zeros - Places,
        length(Int, IntLen),
        append(Int, FracDigits, Digits),
        Frac = ['.'|FracDigits]
    ).

% ~D は整数部を右から 3 桁ごとに , で区切る。区切りは右から数えるので、逆にしてから前から 3 桁ずつ区切る。
group_digits(plain, Int, Int).
group_digits(grouped, Int0, Int) :-
    reverse(Int0, Rev0),
    group_reversed(Rev0, Rev),
    reverse(Rev, Int).

group_reversed([A, B, C, D|Ds], [A, B, C, ','|Rest]) :-
    !,
    group_reversed([D|Ds], Rest).
group_reversed(Ds, Ds).

% ~r と ~R は桁の数を基数にする。無ければ 8（スイートの radix_default が 16 に 20 を期待する）。基数の誤りは
% 書式だけで決まるので、引数を読む前に確かめる。
radix_directive(Case, Count, Args0, Args, PI, St0, St) :-
    default(Count, 8, Radix),
    (   Radix >= 2,
        Radix =< 36
    ->  true
    ;   throw(error(domain_error(radix, Radix), context(PI, _)))
    ),
    next_arg(Args0, PI, X, Args),
    evaluate_integer(X, PI, N),
    '$format_radix'(N, Radix, Case, A),
    emit_atom(A, St0, St).

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
% 項目は text(文字のリスト) か fill(埋める文字)。~|、~+、~n（と改行を出した ~N）で区切りを確定する。
emit_atom(A, St0, St) :-
    atom_chars(A, Chars),
    emit(Chars, St0, St).

emit(Chars, st(Col, Seg, Out), st(Col, [text(Chars)|Seg], Out)).

flush(st(Col, Seg, Out), St) :-
    reverse(Seg, Items),
    place(Items, 0, Col, Out, St).

% 桁は東アジア幅で数える。幅の表は 読み幅/2 と同じ '$char_width'/2（src/unicode_tables.inc）。~| と ~+ は
% 桁止めの位置の決め方だけが違う。埋め方と幅の数え方を 1 つの道で通し、2 つの指示の結果が食い違わないようにする。
column_stop(Kind, Count, st(Col, Seg, Out), St) :-
    reverse(Seg, Items0),
    items_text(Items0, Text),
    column_after(Text, Col, Here),
    stop_target(Kind, Count, Col, Text, Here, Target),
    Pad is max(0, Target - Here),
    (   member(fill(_), Items0)
    ->  Items = Items0
    ;   append(Items0, [fill(' ')], Items)
    ),
    place(Items, Pad, Col, Out, St).

% ~N| は呼び出しの始まりから N 桁目で、N が無ければ今の桁（埋めずに区切りを確定する）。~N+ は前の桁止めから N 桁先で、
% N が無ければ 8 桁先（スイートの default_column_boundary の件）。前の桁止めはふつう区切りの始まりの桁（place/5 が
% 返す桁なので、字が桁止めを越えたときは越えた後の桁）だが、行の頭も桁止めとする（iso-notes「Section 5 format/2,3」の
% ~+ と改行の項）。区切りの字に改行があれば、それが書式の字でも引数の字でも桁 0 から数える。改行は区切りを確定しない
% ので、Col のままでは前の行の桁から数えてしまい、改行の後の桁を column_after/3 で数える ~| と食い違う。
stop_target(absolute, Count, _, _, Here, Target) :-
    default(Count, Here, Target).
stop_target(relative, Count, Col, Text, _, Target) :-
    default(Count, 8, N),
    (   member('\n', Text)
    ->  Base = 0
    ;   Base = Col
    ),
    Target is Base + N.

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

% text と fill の節は第 1 引数の索引ではどちらも [_|_] なので、切らないと text ごとに選択点が残る（0.1.1）。
expand([], _, _, _, []).
expand([text(Text)|Items], I, First, Base, Chars) :-
    !,
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
