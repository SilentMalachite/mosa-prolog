:- use_module(library(format)).
% 出力は with_output_to/2 で原子にして比べる。誤りの Formal は iso-notes の Section 5 format/2 errors。
% 書式の形。
test(fmt_plain, with_output_to(atom(A), format("abc", [])), solutions([A = abc])).
test(fmt_empty, with_output_to(atom(A), format("", [])), solutions([A = ''])).
test(fmt_atom_format, with_output_to(atom(A), format('x~w', [1])), solutions([A = x1])).
test(fmt_chars_format, with_output_to(atom(A), format([x, '~', w], [1])), solutions([A = x1])).
test(fmt_1, with_output_to(atom(A), format("ab~n")), solutions([A = 'ab\n'])).
test(fmt_stream, format(user_error, "x", []), true).
test(fmt_stream_current, with_output_to(atom(A), (current_output(S), format(S, "~w", [s]))), solutions([A = s])).
% ~w、~q、~a。
test(fmt_w, with_output_to(atom(A), format("~w", [f('A', "b")])), solutions([A = 'f(A,[98])'])).
test(fmt_q, with_output_to(atom(A), format("~q", ['A'])), solutions([A = '\'A\''])).
test(fmt_a, with_output_to(atom(A), format("~a", ['ABC'])), solutions([A = 'ABC'])).
test(fmt_a_count, with_output_to(atom(A), format("~3a", [abc])), solutions([A = abc])).
test(fmt_a_nil, with_output_to(atom(A), format("~a", [[]])), solutions([A = '[]'])).
test(fmt_a_inst, format("~a", [_]), error(instantiation_error)).
test(fmt_a_type, format("~a", [42]), error(type_error(atom, 42))).
% ~d。引数は is/2 と同じく評価する。
test(fmt_d, with_output_to(atom(A), format("~d", [123])), solutions([A = '123'])).
test(fmt_d_expr, with_output_to(atom(A), format("~d", [1 + 2])), solutions([A = '3'])).
test(fmt_d_point, with_output_to(atom(A), format("~1d", [123])), solutions([A = '12.3'])).
test(fmt_d_small, with_output_to(atom(A), format("~3d", [5])), solutions([A = '0.005'])).
test(fmt_d_neg, with_output_to(atom(A), format("~3d", [-5])), solutions([A = '-0.005'])).
test(fmt_d_star, with_output_to(atom(A), format("~*d", [2, 1234])), solutions([A = '12.34'])).
test(fmt_d_inst, format("~d", [_]), error(instantiation_error)).
test(fmt_d_type, format("~d", [a]), error(type_error(evaluable, a/0))).
test(fmt_d_float, format("~d", [123.0]), error(type_error(integer, 123.0))).
test(fmt_d_type_nested, format("~d", [foo(bar)]), error(type_error(evaluable, foo/1))).
test(fmt_d_neg_count, format("~*d", [-1, 123]), error(domain_error(not_less_than_zero, -1))).
test(fmt_star_inst, format("~*d", [_, 123]), error(instantiation_error)).
% ~f、~e。
test(fmt_f, with_output_to(atom(A), format("~f", [1.0])), solutions([A = '1.000000'])).
test(fmt_f2, with_output_to(atom(A), format("~2f", [3.14159])), solutions([A = '3.14'])).
test(fmt_f_round, with_output_to(atom(A), format("~3f", [12.3456789])), solutions([A = '12.346'])).
test(fmt_f0, with_output_to(atom(A), format("~0f", [123.456])), solutions([A = '123'])).
test(fmt_f_neg, with_output_to(atom(A), format("~4f", [-1.0e-1])), solutions([A = '-0.1000'])).
test(fmt_f_int, with_output_to(atom(A), format("~2f", [1 + 1])), solutions([A = '2.00'])).
test(fmt_f_type, format("~f", [foo(1)]), error(type_error(evaluable, foo/1))).
test(fmt_e, with_output_to(atom(A), format("~e", [1.0])), solutions([A = '1.000000e+00'])).
test(fmt_e3, with_output_to(atom(A), format("~3e", [1.333333])), solutions([A = '1.333e+00'])).
test(fmt_e0, with_output_to(atom(A), format("~0e", [1.333333])), solutions([A = '1e+00'])).
test(fmt_e_big, with_output_to(atom(A), format("~2e", [12345.0])), solutions([A = '1.23e+04'])).
test(fmt_e_inst, format("~e", [_]), error(instantiation_error)).
% ~s。
test(fmt_s_codes, with_output_to(atom(A), format("~s", [[65, 66, 67]])), solutions([A = 'ABC'])).
test(fmt_s_chars, with_output_to(atom(A), format("~s", [['A', 'B']])), solutions([A = 'AB'])).
test(fmt_s_first, with_output_to(atom(A), format("~2s", ["abc"])), solutions([A = ab])).
test(fmt_s_pad, with_output_to(atom(A), format("~5s", ["abc"])), solutions([A = 'abc  '])).
test(fmt_s_inst, format("~s", [_]), error(instantiation_error)).
test(fmt_s_partial, format("~s", [[65|_]]), error(instantiation_error)).
test(fmt_s_type, format("~s", [42]), error(type_error(list, 42))).
% ~c、~n、~~。
test(fmt_c, with_output_to(atom(A), format("~c", [0'A])), solutions([A = 'A'])).
test(fmt_c_count, with_output_to(atom(A), format("~3c", [0'x])), solutions([A = xxx])).
test(fmt_c_kanji, with_output_to(atom(A), format("~c", [0'漢])), solutions([A = '漢'])).
test(fmt_c_type, format("~c", [a]), error(type_error(evaluable, a/0))).
test(fmt_n, with_output_to(atom(A), format("a~nb", [])), solutions([A = 'a\nb'])).
test(fmt_n_count, with_output_to(atom(A), format("~2n", [])), solutions([A = '\n\n'])).
test(fmt_tilde, with_output_to(atom(A), format("~~", [])), solutions([A = '~'])).
% ~t と ~|。桁は文字数で数える。
test(fmt_column, with_output_to(atom(A), format("~w~t~10|~w", [ab, x])), solutions([A = 'ab        x'])).
test(fmt_col_left, with_output_to(atom(A), format("~w~6|x", [ab])), solutions([A = 'ab    x'])).
test(fmt_col_right, with_output_to(atom(A), format("~t~w~6|", [ab])), solutions([A = '    ab'])).
test(fmt_col_center, with_output_to(atom(A), format("~t~w~t~6|", [ab])), solutions([A = '  ab  '])).
test(fmt_col_spread, with_output_to(atom(A), format("~ta~tb~tc~t~10|", [])), solutions([A = ' a  b  c  '])).
test(fmt_col_fill_char, with_output_to(atom(A), format("~`-t~6|", [])), solutions([A = '------'])).
test(fmt_col_fill_code, with_output_to(atom(A), format("~61t~3|", [])), solutions([A = '==='])).
test(fmt_col_fill_star, with_output_to(atom(A), format("~*t~3|", [0'.])), solutions([A = '...'])).
test(fmt_col_star, with_output_to(atom(A), format("~*|x", [3])), solutions([A = '   x'])).
test(fmt_col_past, with_output_to(atom(A), format("abcdef~t~3|x", [])), solutions([A = abcdefx])).
test(fmt_col_here, with_output_to(atom(A), format("left~tright~|", [])), solutions([A = leftright])).
test(fmt_col_second, with_output_to(atom(A), format("~w~t~4|~t~w~8|", [a, b])), solutions([A = 'a      b'])).
test(fmt_col_after_nl, with_output_to(atom(A), format("abc~nx~t~3|y", [])), solutions([A = 'abc\nx  y'])).
test(fmt_col_kanji, with_output_to(atom(A), format("~w~t~4|x", [漢字])), solutions([A = '漢字  x'])).
% 引数の誤り。
test(fmt_args_short, format("~a", []), error(domain_error(non_empty_list, []))).
test(fmt_args_extra, format("abc", [def]), error(domain_error(empty_list, [def]))).
test(fmt_unknown, format("~x", [a]), error(domain_error(format_directive, x))).
test(fmt_dangling, format("abc~", []), error(domain_error(format_directive, '~'))).
test(fmt_format_inst, format(_, []), error(instantiation_error)).
test(fmt_format_type, format(42, []), error(type_error(list, 42))).
test(fmt_args_inst, format("~d", _), error(instantiation_error)).
test(fmt_args_partial, format("~d", [42|_]), error(instantiation_error)).
test(fmt_args_type, format("~d", 42), error(type_error(list, 42))).
% 誤りのときは何も書かない。
test(fmt_no_partial, with_output_to(atom(A), catch(format("ab~a", [1]), _, true)), solutions([A = ''])).
% '$format_float'/4 は system にある。
test(fmt_float_f, '$format_float'(f, 2, 2.5, A), solutions([A = '2.50'])).
test(fmt_float_e, '$format_float'(e, 1, 2.5, A), solutions([A = '2.5e+00'])).
test(fmt_float_type, '$format_float'(f, 2, 1, _), error(type_error(float, 1))).
test(fmt_float_neg, '$format_float'(f, -1, 1.0, _), error(domain_error(not_less_than_zero, -1))).
