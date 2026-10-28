use_format :- use_module(library(format)).

% 取り込むまで user からは見えない。幅の補助述語も user には出さない。
test(format_hidden, format('hi', []), error(existence_error(procedure, format/2))).
test(format_width_hidden, display_width(26481, _), error(existence_error(procedure, display_width/2))).

% 書式の文字はそのまま出る。
test(format_lit, (use_format, with_output_to(atom(A), format('hi', [])), A == hi), true).

% format/1 は引数の無い format/2。
test(format_one, (use_format, with_output_to(atom(A), format('ab~n')), A == 'ab\n'), true).

% format/3 は指定した流れへ書く。
test(format_stream, (use_format, with_output_to(atom(A), (current_output(S), format(S, '~a', [hi]))), A == hi), true).

% 書式は原子、符号位置のリスト、一文字原子のリスト。
test(format_codes, (use_format, with_output_to(atom(A), format([104, 105], [])), A == hi), true).
test(format_chars, (use_format, with_output_to(atom(A), format(['h', 'i'], [])), A == hi), true).
test(format_quotes, (use_format, with_output_to(atom(A), format("hi", [])), A == hi), true).
test(format_control_var, (use_format, format(_, [])), error(instantiation_error)).
test(format_control_type, (use_format, format(42, [])), error(type_error(text, 42))).
test(format_args_type, (use_format, format('~d', 42)), error(type_error(list, 42))).
test(format_args_var, (use_format, format('~d', _)), error(instantiation_error)).
test(format_args_short, (use_format, format('~a', [])), error(domain_error(format_arguments, []))).
test(format_args_long, (use_format, format('hi', [a])), error(domain_error(format_arguments, [a]))).
test(format_bad, (use_format, format('~x', [])), error(domain_error(format_sequence, x))).
test(format_tilde_end, (use_format, format('~', [])), error(domain_error(format_sequence, '~'))).

% ~~ はチルダを一つ出す。数字が付いても一つ。
test(format_tilde, (use_format, with_output_to(atom(A), format('~~', [])), A == '~'), true).
test(format_tilde_count, (use_format, with_output_to(atom(A), format('~3~', [])), A == '~'), true).

% ~n は改行を一つ出す。引数は消費しない。改行のあとの桁は行頭。
test(format_nl, (use_format, with_output_to(atom(A), format('a~nb', [])), A == 'a\nb'), true).
test(format_nl_column, (use_format, with_output_to(atom(A), format('ab~n~t~a~4|', [xy])), A == 'ab\n  xy'), true).

% 数字は改行の回数。省略すると 1。
test(format_nl_count, (use_format, with_output_to(atom(A), format('~4n', [])), A == '\n\n\n\n'), true).

% ~w は write/1 と同じ字面。
test(format_write, (use_format, with_output_to(atom(A), format('~w', [f(a, b)])), A == 'f(a,b)'), true).

% ~q は writeq/1 と同じ字面。
test(format_quoted, (use_format, with_output_to(atom(A), format('~q', ['ABC'])), A == '\'ABC\''), true).

% ~a は原子を引用せずに書く。
test(format_atom, (use_format, with_output_to(atom(A), format('~a', ['A B'])), A == 'A B'), true).

% 原子でない引数は型エラー。変数は instantiation。
test(format_atom_type, (use_format, format('~a', [42])), error(type_error(atom, 42))).
test(format_atom_var, (use_format, format('~a', [_])), error(instantiation_error)).

% ~c は文字コードを一回書く。数字はその回数。
test(format_code, (use_format, with_output_to(atom(A), format('~c', [0'A])), A == 'A'), true).
test(format_code_count, (use_format, with_output_to(atom(A), format('~8c', [0'A])), A == 'AAAAAAAA'), true).
% * は次の引数を回数にする。
test(format_star, (use_format, with_output_to(atom(A), format('~*c', [3, 0'A])), A == 'AAA'), true).
test(format_code_type, (use_format, format('~c', [a])), error(type_error(integer, a))).
test(format_code_var, (use_format, format('~c', [_])), error(instantiation_error)).

% ~s は符号位置のリスト、一文字原子のリスト、または原子。
test(format_string_codes, (use_format, with_output_to(atom(A), format('~s', [[65, 66, 67]])), A == 'ABC'), true).
test(format_string_chars, (use_format, with_output_to(atom(A), format('~s', [['A', 'B', 'C']])), A == 'ABC'), true).
test(format_string_atom, (use_format, with_output_to(atom(A), format('~s', ['ABC'])), A == 'ABC'), true).
% 数字は幅。長いときは切り、短いときは右を空白で埋める。
test(format_string_cut, (use_format, with_output_to(atom(A), format('~3s', [[65, 66, 67, 68]])), A == 'ABC'), true).
test(format_string_pad, (use_format, with_output_to(atom(A), format('~6s', [[65, 66, 67]])), A == 'ABC   '), true).

% ~d は整数を十進で書く。引数は算術式。
test(format_decimal, (use_format, with_output_to(atom(A), format('~d', [123])), A == '123'), true).
test(format_decimal_expr, (use_format, with_output_to(atom(A), format('~d', [20 + 3])), A == '23'), true).
% 数字は小数点の位置。123 を 1 桁ずらすと 12.3。
test(format_decimal_places, (use_format, with_output_to(atom(A), format('~1d', [123])), A == '12.3'), true).
test(format_decimal_neg, (use_format, with_output_to(atom(A), format('~2d', [-123])), A == '-1.23'), true).
test(format_decimal_zeros, (use_format, with_output_to(atom(A), format('~2d', [5])), A == '0.05'), true).
test(format_decimal_float, (use_format, format('~d', [123.0])), error(type_error(integer, 123.0))).

% ~f は小数点以下 6 桁が既定。数字はその桁数。整数も float にする。
test(format_float, (use_format, with_output_to(atom(A), format('~f', [1.0])), A == '1.000000'), true).
test(format_float_places, (use_format, with_output_to(atom(A), format('~3f', [12.3456789])), A == '12.346'), true).
test(format_float_neg, (use_format, with_output_to(atom(A), format('~4f', [-1.0e-1])), A == '-0.1000'), true).
test(format_float_zero, (use_format, with_output_to(atom(A), format('~0f', [123.456])), A == '123'), true).
test(format_float_int, (use_format, with_output_to(atom(A), format('~2f', [29])), A == '29.00'), true).

% ~e は指数表記。既定は小数 6 桁で、指数は符号付き 2 桁。
test(format_exp, (use_format, with_output_to(atom(A), format('~e', [1.333333])), A == '1.333333e+00'), true).
test(format_exp_places, (use_format, with_output_to(atom(A), format('~3e', [16.66666])), A == '1.667e+01'), true).
test(format_exp_zero, (use_format, with_output_to(atom(A), format('~0e', [1.333333])), A == '1e+00'), true).
test(format_exp_neg, (use_format, with_output_to(atom(A), format('~e', [-0.001])), A == '-1.000000e-03'), true).

% ~t は余白の位置。~N| は桁 N まで埋め、~t が無ければ右端の手前に空白を置く。
test(format_tab_right, (use_format, with_output_to(atom(A), format('~t~a~8|', [abcd])), A == '    abcd'), true).
test(format_tab_skip, (use_format, with_output_to(atom(A), format('~6|~a', [abcd])), A == '      abcd'), true).
% ~N+ は直前の桁から N 先。省略は 8。左寄せは文字の後ろに ~t を置く。
test(format_tab_left, (use_format, with_output_to(atom(A), format('~|~a~t~8+', [abcd])), A == 'abcd    '), true).
test(format_tab_center, (use_format, with_output_to(atom(A), format('~t~a~t~8+', [abcd])), A == '  abcd  '), true).
% 東アジアの広い文字は幅 2。埋め文字の数字は符号位置。
test(format_tab_wide, (use_format, with_output_to(atom(A), format('~t~a~4|', [東])), A == '  東'), true).
test(format_tab_fill, (use_format, with_output_to(atom(A), format('~8594t~8|', [])), A == '→→→→→→→→'), true).
% 埋め文字が幅 2 のとき、桁数で数える。26481 は東。
test(format_tab_wide_fill, (use_format, with_output_to(atom(A), format('~26481t~4|', [])), A == '東東'), true).
