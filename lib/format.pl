:- module(format, [format/1, format/2, format/3]).

% 制御列が無い文字はそのまま書く。~t の余白は次の桁位置まで溜めてから分ける。
format(Control) :- format(Control, []).

format(Stream, Control, Args) :- with_output_to(Stream, format(Control, Args)).

format(Control, Args) :-
    control_codes(Control, Codes),
    arg_list(Args),
    emit(Codes, Args, 0, []).

arg_list(Args) :- var(Args), !, throw(error(instantiation_error, _)).
arg_list(Args) :- arg_list_ok(Args), !.
arg_list(Args) :- throw(error(type_error(list, Args), _)).

arg_list_ok([]) :- !.
arg_list_ok([_ | Tail]) :- arg_list(Tail).

control_codes(Control, _) :- var(Control), !, throw(error(instantiation_error, _)).
control_codes(Control, Codes) :- atom(Control), !, atom_codes(Control, Codes).
control_codes(Control, Codes) :- control_list(Control, Codes).

control_list([], []) :- !.
control_list([Head | Tail], [Code | Codes]) :- !,
    control_item(Head, Code),
    control_list(Tail, Codes).
control_list(Control, _) :- var(Control), !, throw(error(instantiation_error, _)).
control_list(Control, _) :- throw(error(type_error(text, Control), _)).

control_item(Item, _) :- var(Item), !, throw(error(instantiation_error, _)).
control_item(Item, Code) :- one_code(Item, Code), !.
control_item(Item, _) :- throw(error(type_error(text, Item), _)).

emit([], [], _, Pending) :- !, flush_pad(Pending, 0).
emit([], Args, _, _) :- throw(error(domain_error(format_arguments, Args), _)).
emit([0'~ | Rest], Args, Col, Pending) :- !,
    take_count(Rest, Raw, After0),
    ( After0 = [Op | After] ->
        resolve_param(Raw, Args, Count, NextArgs),
        run_op(Op, Count, After, NextArgs, Col, Pending)
    ; throw(error(domain_error(format_sequence, '~'), _))
    ).
emit([Code | Rest], Args, Col, Pending) :-
    add_text([Code], Pending, Next),
    emit(Rest, Args, Col, Next).

% 数字が無ければ none。* は次の引数。~ の次の文字は操作なので、桁の残りには含めない。
take_count([0'* | Rest], star, Rest) :- !.
take_count(Codes, Count, Rest) :- read_digits(Codes, 0, 0, Count, Rest).

resolve_param(star, [Expr | Args], Value, Args) :- !,
    Number is Expr,
    star_value(Number, Value).
resolve_param(Count, Args, Count, Args).

star_value(Value, Value) :- integer(Value), Value >= 0, !.
star_value(Value, _) :- integer(Value), !, throw(error(domain_error(not_less_than_zero, Value), _)).
star_value(Value, _) :- throw(error(type_error(integer, Value), _)).

read_digits([Code | Rest], Acc, _, Count, After) :-
    Code >= 0'0, Code =< 0'9, !,
    Next is Acc * 10 + (Code - 0'0),
    read_digits(Rest, Next, 1, Count, After).
read_digits(Rest, _, 0, none, Rest) :- !.
read_digits(Rest, Acc, _, Acc, Rest).

run_op(Op, _, _, [], _, _) :-
    needs_arg(Op), !,
    throw(error(domain_error(format_arguments, []), _)).
run_op(0'~, _, Rest, Args, Col, Pending) :- !,
    add_text([0'~], Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0'n, Count, Rest, Args, _, Pending) :- !,
    flush_pad(Pending, 0),
    repeat_of(Count, 1, Times),
    new_lines(Times),
    emit(Rest, Args, 0, []).
run_op(0'w, _, Rest, [Term | Args], Col, Pending) :- !,
    captured(write(Term), Codes),
    add_text(Codes, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0'q, _, Rest, [Term | Args], Col, Pending) :- !,
    captured(writeq(Term), Codes),
    add_text(Codes, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0'a, _, Rest, [Term | Args], Col, Pending) :- !,
    plain_atom(Term),
    captured(write(Term), Codes),
    add_text(Codes, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0'c, Count, Rest, [Code | Args], Col, Pending) :- !,
    repeat_of(Count, 1, Times),
    captured(put_times(Times, Code), Codes),
    add_text(Codes, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0'e, Count, Rest, [Expr | Args], Col, Pending) :- !,
    Value is float(Expr),
    repeat_of(Count, 6, Places),
    captured(write_exp(Value, Places), Codes),
    add_text(Codes, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0'f, Count, Rest, [Expr | Args], Col, Pending) :- !,
    Value is float(Expr),
    repeat_of(Count, 6, Places),
    captured(write_fixed(Value, Places), Codes),
    add_text(Codes, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0'd, Count, Rest, [Expr | Args], Col, Pending) :- !,
    Value is Expr,
    decimal_integer(Value),
    repeat_of(Count, 0, Places),
    captured(write_decimal(Value, Places), Codes),
    add_text(Codes, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0's, Count, Rest, [Text | Args], Col, Pending) :- !,
    text_codes(Text, Codes),
    take_width(Count, Codes, Fitted),
    captured(put_codes(Fitted), Out),
    add_text(Out, Pending, Next),
    emit(Rest, Args, Col, Next).
run_op(0't, Count, Rest, Args, Col, Pending) :- !,
    fill_of(Count, Fill),
    add_tab(Pending, Fill, Next),
    emit(Rest, Args, Col, Next).
run_op(0'|, Count, Rest, Args, Col, Pending) :- !,
    pending_width(Pending, Width),
    column_target(Count, Col, Width, Target),
    Pad is Target - Col - Width,
    flush_pad(Pending, Pad),
    emit(Rest, Args, Target, []).
run_op(0'+, Count, Rest, Args, Col, Pending) :- !,
    repeat_of(Count, 8, Dist),
    pending_width(Pending, Width),
    Pad is Dist - Width,
    flush_pad(Pending, Pad),
    Target is Col + Dist,
    emit(Rest, Args, Target, []).
run_op(Code, _, _, _, _, _) :-
    char_code(Name, Code),
    throw(error(domain_error(format_sequence, Name), _)).

repeat_of(none, Default, Default) :- !.
repeat_of(Times, _, Times).

needs_arg(0'w).
needs_arg(0'q).
needs_arg(0'a).
needs_arg(0'c).
needs_arg(0'e).
needs_arg(0'f).
needs_arg(0'd).
needs_arg(0's).

% write/1 は数も書く。~a は原子だけ。
plain_atom(Term) :- var(Term), !, throw(error(instantiation_error, _)).
plain_atom(Term) :- atom(Term), !.
plain_atom(Term) :- throw(error(type_error(atom, Term), _)).

new_lines(0) :- !.
new_lines(Times) :- Times > 0, nl, Left is Times - 1, new_lines(Left).

put_times(0, _) :- !.
put_times(Times, Code) :- Times > 0, put_code(Code), Left is Times - 1, put_times(Left, Code).

put_codes([]).
put_codes([Item | Rest]) :- one_code(Item, Code), put_code(Code), put_codes(Rest).

one_code(Item, Item) :- integer(Item), !.
one_code(Item, Code) :- atom(Item), atom_codes(Item, [Code]), !.

text_codes(Text, Codes) :- atom(Text), !, atom_codes(Text, Codes).
text_codes(Text, Text).

take_width(none, Codes, Codes) :- !.
take_width(Width, Codes, Fitted) :- pad_codes(Width, Codes, Fitted).

% 1 以上 10 未満に揃えてから %.Ne。指数は少なくとも 2 桁。
write_exp(Value, Places) :-
    sign_abs(Value, Abs),
    exp_parts(Abs, Places, Mantissa, Exp),
    write_fixed(Mantissa, Places),
    write_exp_suffix(Exp).

exp_parts(Abs, _, 0.0, 0) :- Abs =:= 0, !.
exp_parts(Abs, Places, Mantissa, Exp) :-
    Guess is floor(log(Abs) / log(10.0)),
    Raw is Abs / (10.0 ^ Guess),
    normalize_exp(Raw, Guess, Places, Mantissa, Exp).

normalize_exp(Raw, Guess, Places, Mantissa, Exp) :-
    Scale is 10 ^ Places,
    Scaled is round(Raw * Scale),
    Limit is Scale * 10,
    ( Scaled >= Limit -> Mantissa is Raw / 10.0, Exp is Guess + 1
    ; Raw < 1.0 -> Mantissa is Raw * 10.0, Exp is Guess - 1
    ; Mantissa = Raw, Exp = Guess
    ).

write_exp_suffix(Exp) :-
    put_code(0'e),
    exp_sign(Exp, Abs),
    ( Abs < 10 -> put_code(0'0) ; true ),
    write(Abs).

exp_sign(Exp, Abs) :- Exp < 0, !, put_code(0'-), Abs is -Exp.
exp_sign(Exp, Exp) :- put_code(0'+).

% printf の %.Nf。丸めは round/1。0 桁のときは小数点を出さない。
write_fixed(Value, 0) :- !,
    sign_abs(Value, Abs),
    Rounded is round(Abs),
    write(Rounded).
write_fixed(Value, Places) :-
    Scale is 10 ^ Places,
    sign_abs(Value, Abs),
    Scaled is round(Abs * Scale),
    Whole is Scaled // Scale,
    Frac is Scaled mod Scale,
    write(Whole),
    put_code(0'.),
    write_frac(Frac, Places).

% is/2 は float も返す。~d は整数だけ。
decimal_integer(Value) :- integer(Value), !.
decimal_integer(Value) :- throw(error(type_error(integer, Value), _)).

% 小数点は右から Places 桁。符号は点の外に置く。
write_decimal(Value, 0) :- !, write(Value).
write_decimal(Value, Places) :-
    sign_abs(Value, Abs),
    Scale is 10 ^ Places,
    Whole is Abs // Scale,
    Frac is Abs mod Scale,
    write(Whole),
    put_code(0'.),
    write_frac(Frac, Places).

sign_abs(Value, Abs) :- Value < 0, !, put_code(0'-), Abs is -Value.
sign_abs(Value, Value).

write_frac(Frac, Places) :-
    number_codes(Frac, Digits),
    length(Digits, Have),
    Pad is Places - Have,
    zero_pad(Pad),
    put_codes(Digits).

zero_pad(0) :- !.
zero_pad(Pad) :- Pad > 0, put_code(0'0), Next is Pad - 1, zero_pad(Next).

% 幅に満たない分は空白。超えた分は捨てる。32 は空白の符号位置。
pad_codes(0, _, []) :- !.
pad_codes(Width, [], [32 | Rest]) :- !,
    Next is Width - 1,
    pad_codes(Next, [], Rest).
pad_codes(Width, [Code | Codes], [Code | Rest]) :-
    Width > 0,
    Next is Width - 1,
    pad_codes(Next, Codes, Rest).

% 書き出した字面を符号位置で受け、桁が決まるまで行に溜める。
captured(Goal, Codes) :- with_output_to(codes(Codes), Goal).

fill_of(none, 32) :- !.
fill_of(Code, Code).

column_target(none, Col, Width, Target) :- !, Target is Col + Width.
column_target(Column, _, _, Column).

add_text(Codes, Pending, Next) :-
    split_last(Pending, Front, Last),
    ( Last = s(Have) ->
        join(Have, Codes, Joined),
        join(Front, [s(Joined)], Next)
    ; Last = none -> Next = [s(Codes)]
    ; join(Pending, [s(Codes)], Next)
    ).

add_tab(Pending, Fill, Next) :- join(Pending, [t(Fill)], Next).

split_last([], [], none) :- !.
split_last([Only], [], Only) :- !.
split_last([Head | Tail], [Head | Front], Last) :- split_last(Tail, Front, Last).

join([], Right, Right).
join([Head | Tail], Right, [Head | Rest]) :- join(Tail, Right, Rest).

% タブが無い余白は右端の手前。タブがあるときは後ろのタブが 1 つ多く取る。
flush_pad(Pending, Pad) :-
    ( Pad > 0 -> Use = Pad ; Use = 0 ),
    tab_count(Pending, Tabs),
    ( Tabs =:= 0 ->
        texts(Pending, Text),
        pad_codes(Use, [], Spaces),
        join(Text, Spaces, Codes)
    ; render_tabs(Pending, Use, Tabs, Codes)
    ),
    put_codes(Codes).

render_tabs(Pending, Pad, Tabs, Codes) :-
    Base is Pad // Tabs,
    Extra is Pad mod Tabs,
    Early is Tabs - Extra,
    lay(Pending, Base, Early, Codes).

lay([], _, _, []).
lay([s(Text) | Rest], Base, Early, Codes) :-
    lay(Rest, Base, Early, More),
    join(Text, More, Codes).
lay([t(Fill) | Rest], Base, Early, Codes) :-
    tab_span(Base, Early, Span, Next),
    fill_columns(Span, Fill, Pads),
    lay(Rest, Base, Next, More),
    join(Pads, More, Codes).

tab_span(Base, Early, Base, Next) :- Early > 0, !, Next is Early - 1.
tab_span(Base, _, Span, 0) :- Span is Base + 1.

% 幅 2 の埋め文字は 2 桁使う。半端は空白で桁に合わせる。
fill_columns(0, _, []) :- !.
fill_columns(Span, Fill, [Fill | Rest]) :-
    display_width(Fill, One),
    One =< Span, !,
    Left is Span - One,
    fill_columns(Left, Fill, Rest).
fill_columns(Span, _, Pads) :- pad_codes(Span, [], Pads).

texts([], []).
texts([s(Text) | Rest], Codes) :- texts(Rest, More), join(Text, More, Codes).
texts([t(_) | Rest], Codes) :- texts(Rest, Codes).

tab_count([], 0).
tab_count([t(_) | Rest], Count) :- !, tab_count(Rest, More), Count is More + 1.
tab_count([_ | Rest], Count) :- tab_count(Rest, Count).

pending_width([], 0).
pending_width([s(Text) | Rest], Width) :-
    code_width(Text, Here),
    pending_width(Rest, More),
    Width is Here + More.
pending_width([t(_) | Rest], Width) :- pending_width(Rest, Width).

code_width([], 0).
code_width([Code | Rest], Width) :-
    display_width(Code, One),
    code_width(Rest, More),
    Width is One + More.
