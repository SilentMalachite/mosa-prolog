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
% レビュー L7。最小の整数の abs はあふれる。符号を字から外して桁を数える。
test(l7_fmt_d_min_integer, (current_prolog_flag(min_integer, M), with_output_to(atom(A), format("~2d", [M]))), solutions([A = '-92233720368547758.08'])).
test(l7_fmt_d_max_integer, (current_prolog_flag(max_integer, M), with_output_to(atom(A), format("~2d", [M]))), solutions([A = '92233720368547758.07'])).
% レビュー L3。要素が循環していても引数のリストとして受け取る。
test(l3_fmt_cyclic_arg, (X = f(X), with_output_to(atom(A), format("~w", [X]))), solutions([A = 'f(_)'])).
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
% フェーズ11のタスク8。~E は ~e の指数の字を E にする。~g と ~G は C の %g と %G と同じく、指数の大きさで小数点表記と
% 指数表記を選び、末尾の 0 を落とす。桁の数は有効数字の桁数で、0 は 1 と同じになる。
test(fmt_E, with_output_to(atom(A), format("~E", [1.333333])), solutions([A = '1.333333E+00'])).
test(fmt_E_expr, with_output_to(atom(A), format("~E", [1.333333+1])), solutions([A = '2.333333E+00'])).
test(fmt_E_places, (with_output_to(atom(A), format("~3E", [1.333333])), with_output_to(atom(B), format("~*E", [3, 1.333333]))), solutions([(A = '1.333E+00', B = '1.333E+00')])).
test(fmt_E_zero, with_output_to(atom(A), format("~0E", [1.333333])), solutions([A = '1E+00'])).
test(fmt_g, with_output_to(atom(A), format("~g", [0.00000123])), solutions([A = '1.23e-06'])).
test(fmt_g_places, with_output_to(atom(A), format("~2g", [0.00000123])), solutions([A = '1.2e-06'])).
test(fmt_g_fixed, with_output_to(atom(A), format("~g", [0.39265e+3])), solutions([A = '392.65'])).
test(fmt_g_fixed_places, with_output_to(atom(A), format("~2g", [0.39265e+3])), solutions([A = '3.9e+02'])).
test(fmt_g_zero, with_output_to(atom(A), format("~0g", [0.39265e+3])), solutions([A = '4e+02'])).
test(fmt_G, with_output_to(atom(A), format("~G", [0.00000123])), solutions([A = '1.23E-06'])).
test(fmt_G_places, with_output_to(atom(A), format("~2G", [0.39265e+3])), solutions([A = '3.9E+02'])).
test(fmt_G_fixed, with_output_to(atom(A), format("~G", [0.39265e+3])), solutions([A = '392.65'])).
% %g の固定と指数の切り替えと符号を 3 つの OS の CI で留める。-0.0 は符号を残し、精度 6 の指数が 6 なら指数の形にする。
test(fmt_g_switch, with_output_to(atom(A), format("~g|~G|~g", [-0.0, 1234567.0, 100000.0])), solutions([A = '-0|1.23457E+06|100000'])).
test(fmt_g_inf, (X is inf, with_output_to(atom(A), format("~g", [X]))), solutions([A = '1.0Inf'])).
% 無限大と NaN は大文字の指示でも writeq と同じ字面のままにする（Inf と NaN を大文字にしない）。
test(fmt_G_inf, (X is -inf, Y is nan, with_output_to(atom(A), format("~G ~E ~G", [X, X, Y]))), solutions([A = '-1.0Inf -1.0Inf 1.5NaN'])).
test(fmt_g_big, format("~g", [2^2000]), error(evaluation_error(float_overflow))).
test(fmt_g_errors_inst, format("~g", [_]), error(instantiation_error)).
test(fmt_g_errors_star_inst, format("~*G", [5, _]), error(instantiation_error)).
test(fmt_g_errors_evaluable, format("~g", [foo(bar)]), error(type_error(evaluable, foo/1))).
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
% SPEC 第4節。漢字 は幅 4 なので桁 4 で埋め草は無い。フェーズ4の文字数で数えた期待値 '漢字  x' を Task 13 で直した。
test(fmt_col_kanji, with_output_to(atom(A), format("~w~t~4|x", [漢字])), solutions([A = '漢字x'])).
% ~+（フェーズ11のタスク9）。前の桁止めから N 桁（無ければ 8）先に桁止めを置く。埋め方は ~| と同じ。入力と期待値は
% スイートの format_2・format_3 の件と同じ。
test(fmt_plus, (with_output_to(atom(A), format("~|~a~t~+", [abcd])), with_output_to(atom(B), format("~|~a~t~8+", [abcd])), with_output_to(atom(C), format("~t~a~t~8+", [abcd]))), solutions([(A = 'abcd    ', B = 'abcd    ', C = '  abcd  ')])).
test(fmt_plus_text, (with_output_to(atom(A), format("left~10+next", [])), with_output_to(atom(B), format("~tright~10+next", [])), with_output_to(atom(C), format("^~|~ta~tb~tc~t~10+$", []))), solutions([(A = 'left      next', B = '     rightnext', C = '^ a  b  c  $')])).
test(fmt_plus_chain, (with_output_to(atom(A), format("~*+.~n~3+.~n~*+.~n", [3, 3])), with_output_to(atom(B), format("~t~a~10|~t~a~t~13+~a~t~10+", ['Alpha', 'and', 'Omega']))), solutions([(A = '   .\n   .\n   .\n', B = '     Alpha     and     Omega     ')])).
test(fmt_plus_radix, with_output_to(atom(A), format("~`0t~2r~16+", [0xFF])), solutions([A = '0000000011111111'])).
% 区切りの字が桁止めを越えれば、~| と同じく埋めない。
test(fmt_plus_past, with_output_to(atom(A), format("abcdefghij~t~8+x", [])), solutions([A = abcdefghijx])).
% 行の頭も桁止めなので、区切りの字に改行があれば ~+ は行の頭（桁 0）から数える（iso-notes「Section 5 format/2,3」）。
% 改行が書式の字か引数の字か、~N が改行を出さなかったかによらない。~| と ~n で書いたときと同じ字面になる。
test(fmt_plus_newline, with_output_to(atom(A), format("~5|ab\n~3+.", [])), solutions([A = '     ab\n   .'])).
test(fmt_plus_newline_N, with_output_to(atom(A), format("~5|ab\n~N~3+.", [])), solutions([A = '     ab\n   .'])).
test(fmt_plus_newline_table, with_output_to(atom(A), format("~w~t~10+~w\n~w~t~10+~w\n", [a, b, c, d])), solutions([A = 'a         b\nc         d\n'])).
% 引数の字の改行でも桁 0 から数えるので、cd の後は桁 3 まで 1 桁埋める（SWI-Prolog の実装とは違う。iso-notes「Section 5 format/2,3」）。
test(fmt_plus_newline_arg, with_output_to(atom(A), format("~5|~w~3+.", ['ab\ncd'])), solutions([A = '     ab\ncd .'])).
% 今の振る舞いを留める。~| は改行の前の埋め所も埋める。字が桁止めを越えた後の ~+ は、字の終わりの桁から数える。
test(fmt_plus_newline_bar, with_output_to(atom(A), format("~tab\ncd~6|.", [])), solutions([A = '    ab\ncd.'])).
test(fmt_plus_past_next, with_output_to(atom(A), format("abcdefghij~t~8+x~t~4+|", [])), solutions([A = 'abcdefghijx   |'])).
% 今の振る舞いを留める。~n と、改行を書く ~N（ここでは桁 2 にあるので書く）は区切りを確定するので、その前の ~t は
% 埋めず、~6| は cd の後を埋める（書式の字の改行とも SWI-Prolog とも違う。iso-notes「Section 5 format/2,3」）。
test(fmt_bar_after_n, with_output_to(atom(A), format("~tab~ncd~6|.", [])), solutions([A = 'ab\ncd    .'])).
test(fmt_bar_after_N, with_output_to(atom(A), format("~tab~Ncd~6|.", [])), solutions([A = 'ab\ncd    .'])).
% ~i、~k、~W、~p、~N（フェーズ11のタスク6）。入力と期待値はスイートの format_2・format_3 の件と同じ。
test(fmt_i, with_output_to(atom(A), format("~a~i~a", [a, b, c])), solutions([A = ac])).
test(fmt_i_count, with_output_to(atom(A), format("~a~3i~a", [a, b, c])), solutions([A = ac])).
test(fmt_i_star, with_output_to(atom(A), format("~a~*i~a", [a, 3, b, c])), solutions([A = ac])).
test(fmt_k, with_output_to(atom(A), format("~k", ['A' + 'B'])), solutions([A = '+(\'A\',\'B\')'])).
test(fmt_k_prefix, with_output_to(atom(A), format("~k", [(:- a)])), solutions([A = ':-(a)'])).
test(fmt_W, with_output_to(atom(A), format("~W", [a + 'B', [quoted(true)]])), solutions([A = 'a+\'B\''])).
test(fmt_W_canonical, with_output_to(atom(A), format("~W", [a + 'B', [quoted(true), ignore_ops(true)]])), solutions([A = '+(a,\'B\')'])).
test(fmt_W_bad, format("~W", [a, foo]), error(type_error(list, foo))).
test(fmt_W_short, format("~W", [a]), error(domain_error(non_empty_list, []))).
test(fmt_W_no_output, with_output_to(atom(A), catch(format("ab~W", [a, foo]), _, true)), solutions([A = ''])).
test(fmt_p, with_output_to(atom(A), format("~p", [42])), solutions([A = '42'])).
% portray/1 は fmt_portray_me だけに合うので、このファイルのほかの print と ~p の字面は変わらない。
portray(fmt_portray_me) :- write(seen).
test(fmt_p_portray, with_output_to(atom(A), format("~p", [f(fmt_portray_me)])), solutions([A = 'f(seen)'])).
test(fmt_N, with_output_to(atom(A), format("~Nbegin~N~Nend", [])), solutions([A = 'begin\nend'])).
test(fmt_N_count, with_output_to(atom(A), format("~Nbegin~3N~Nend", [])), solutions([A = 'begin\nend'])).
% 行の途中から始まる ~N は改行を出す（レビューの焦点 1）。行の頭から始まれば出さない。
test(fmt_N_midline, with_output_to(atom(A), (write(x), format("~Nb", []))), solutions([A = 'x\nb'])).
test(fmt_N_line_start, with_output_to(atom(A), (write('x\n'), format("~Nb", []))), solutions([A = 'x\nb'])).
test(fmt_N_stream, with_output_to(atom(A), (write(x), current_output(S), format(S, "~Nb", []))), solutions([A = 'x\nb'])).
test(fmt_N_after_text_nl, with_output_to(atom(A), format("a\n~Nb~n~Nc", [])), solutions([A = 'a\nb\nc'])).
% ~N の改行も ~n と同じく区切りを確定するので、後の ~| は改行の後から数える。
test(fmt_N_column, with_output_to(atom(A), format("~tab~N~w~t~4|x", [c])), solutions([A = 'ab\nc   x'])).
% 行の頭の ~N は区切りを確定しないので、前の ~t は後の ~| で埋める。
test(fmt_N_keeps_fill, with_output_to(atom(A), format("~t~N~w~4|", [a])), solutions([A = '   a'])).
test(fmt3_input_stream, format(user_input, "x", []), error(permission_error(output, stream, user_input))).
% '$stream_column'/2 は put_char/2 と同じ検査をして、最後の改行から書いた字の数を返す。
test(stream_column, with_output_to(atom(_), (write(abc), current_output(S), '$stream_column'(S, C))), solutions([C = 3])).
test(stream_column_line, with_output_to(atom(_), (write('ab\nc'), current_output(S), '$stream_column'(S, C))), solutions([C = 1])).
test(stream_column_inst, '$stream_column'(_, _), error(instantiation_error)).
test(stream_column_domain, '$stream_column'(foo, _), error(domain_error(stream_or_alias, foo))).
test(stream_column_input, '$stream_column'(user_input, _), error(permission_error(output, stream, user_input))).
% ~r、~R、~D（フェーズ11のタスク7）。入力と期待値はスイートの format_2・format_3 の件と同じ。基数の既定は 8
% （スイートの radix_default）。多倍長と負の数も書く（レビューの焦点 2）。
test(fmt_r, with_output_to(atom(A), format("~16r", [127])), solutions([A = '7f'])).
test(fmt_R, with_output_to(atom(A), format("~16R", [127])), solutions([A = '7F'])).
test(fmt_r_default, (with_output_to(atom(A), format("~r", [16])), with_output_to(atom(B), format("~R", [16]))), solutions([(A = '20', B = '20')])).
test(fmt_r_36, with_output_to(atom(A), format("~36r", [35])), solutions([A = z])).
test(fmt_r_big, with_output_to(atom(A), format("~16r", [2^100])), solutions([A = '10000000000000000000000000'])).
test(fmt_r_neg, with_output_to(atom(A), format("~16r", [-255])), solutions([A = '-ff'])).
test(fmt_r_errors_radix_0, format("~0r", [16]), error(domain_error(radix, 0))).
test(fmt_r_errors_radix_1, format("~1R", [16]), error(domain_error(radix, 1))).
test(fmt_r_errors_radix_37, format("~37r", [16]), error(domain_error(radix, 37))).
test(fmt_r_errors_inst, format("~r", [_]), error(instantiation_error)).
test(fmt_r_errors_float, format("~r", [123.0]), error(type_error(integer, 123.0))).
test(fmt_r_errors_evaluable, format("~r", [foo(bar)]), error(type_error(evaluable, foo/1))).
% 誤りのときは何も書かない（レビューの焦点 3）。
test(fmt_r_atomic, with_output_to(atom(A), catch(format("ab~r", [foo]), _, true)), solutions([A = ''])).
test(fmt_D, with_output_to(atom(A), format("~D", [123456789])), solutions([A = '123,456,789'])).
test(fmt_D_places, (with_output_to(atom(A), format("~2D", [123456789])), with_output_to(atom(B), format("~*D", [2, 123456789]))), solutions([(A = '1,234,567.89', B = '1,234,567.89')])).
test(fmt_D_short, with_output_to(atom(A), format("~D", [123])), solutions([A = '123'])).
test(fmt_D_neg, with_output_to(atom(A), format("~D", [-1234567])), solutions([A = '-1,234,567'])).
test(fmt_D_big, with_output_to(atom(A), format("~D", [2^70])), solutions([A = '1,180,591,620,717,411,303,424'])).
test(fmt_D_small, with_output_to(atom(A), format("~2D", [5])), solutions([A = '0.05'])).
test(fmt_D_errors_inst, format("~D", [_]), error(instantiation_error)).
test(fmt_D_errors_float, format("~D", [123.0]), error(type_error(integer, 123.0))).
test(fmt_D_errors_neg_count, format("~*D", [-1, 123]), error(domain_error(not_less_than_zero, -1))).
% ~D の区切りの端を見る。小数部が区切りの 3 桁にまたがる数、負の数のちょうど 2 組、int64 の最小の数。
% 最小の数の abs は int64 にあふれるので、符号を字から外す道を見る。
test(fmt_D_places_grouped, with_output_to(atom(A), format("~4D", [123456789])), solutions([A = '12,345.6789'])).
test(fmt_D_neg_groups, with_output_to(atom(A), format("~D", [-123456])), solutions([A = '-123,456'])).
test(fmt_D_min_integer, (current_prolog_flag(min_integer, M), with_output_to(atom(A), format("~D", [M]))), solutions([A = '-9,223,372,036,854,775,808'])).
% '$format_radix'/4 は system にある。format を通らない誤り（基数と英字の大きさ）はここで見る。
test(format_radix_lower, '$format_radix'(255, 16, lower, A), solutions([A = ff])).
test(format_radix_upper, '$format_radix'(-255, 16, upper, A), solutions([A = '-FF'])).
test(format_radix_big_upper, (X is 36^20 - 1, '$format_radix'(X, 36, upper, A)), solutions([A = 'ZZZZZZZZZZZZZZZZZZZZ'])).
test(format_radix_min_integer, (current_prolog_flag(min_integer, M), '$format_radix'(M, 2, lower, A), atom_length(A, L)), solutions([L = 65])).
test(format_radix_inst, '$format_radix'(_, 16, lower, _), error(instantiation_error)).
test(format_radix_inst_radix, '$format_radix'(1, _, lower, _), error(instantiation_error)).
test(format_radix_inst_case, '$format_radix'(1, 16, _, _), error(instantiation_error)).
test(format_radix_type, '$format_radix'(1.0, 16, lower, _), error(type_error(integer, 1.0))).
test(format_radix_type_radix, '$format_radix'(1, a, lower, _), error(type_error(integer, a))).
test(format_radix_domain, '$format_radix'(1, 37, lower, _), error(domain_error(radix, 37))).
test(format_radix_domain_big, (X is 2^100, '$format_radix'(1, X, lower, _)), error(domain_error(radix, 1267650600228229401496703205376))).
test(format_radix_case, '$format_radix'(1, 16, middle, _), error(domain_error(letter_case, middle))).
test(format_radix_case_type, '$format_radix'(1, 16, 42, _), error(domain_error(letter_case, 42))).
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
test(fmt_float_E, '$format_float'('E', 1, 2.5, A), solutions([A = '2.5E+00'])).
test(fmt_float_g, ('$format_float'(g, 3, 2.5, A), '$format_float'('G', 2, 1.5e-7, B)), solutions([(A = '2.5', B = '1.5E-07')])).
test(format_float_kind, '$format_float'(x, 6, 1.0, _), error(domain_error(format_directive, x))).
% レビュー L20。~d は引数を is/2 と同じく評価するので、循環する式の誤りも is/2 のものになる。
test(l20_fmt_cyclic_d, (X = X+1, format("~d", [X])), error(resource_error(cyclic_term))).
test(l20_fmt_cyclic_e, (X = X+1, format("~e", [X])), error(resource_error(cyclic_term))).
test(l20_fmt_cyclic_column, (X = 1+X, format("~*c", [X, 0'a])), error(resource_error(cyclic_term))).
% レビュー L21。背骨が循環するリストはリストでなく、末尾がリストでないものと同じく type_error(list, …) になる。
% 誤りの項は循環するので期待値には頭の側だけを書き、それが渡したリストそのものであることは *_culprit で見る。
test(l21_fmt_args_cyclic, (X = [a|X], format("~w", X)), error(type_error(list, [a|_]))).
test(l21_fmt_args_culprit, (X = [a|X], catch(format("~w", X), error(type_error(list, C), _), true), C == X), true).
test(l21_fmt_args_lasso, (X = [b, c|X], format("~w~w", [a|X])), error(type_error(list, [a, b, c|_]))).
test(l21_fmt_args_lasso_culprit, (X = [b|X], L = [a|X], catch(format("~w", L), error(type_error(list, C), _), true), C == L), true).
test(l21_fmt_args_cyclic_var, (X = [_|X], format("~w", X)), error(type_error(list, [_|_]))).
test(l21_fmt1_cyclic, (X = [0'a|X], format(X)), error(type_error(list, [0'a|_]))).
test(l21_fmt_format_codes, (X = [0'a|X], format(X, [])), error(type_error(list, [0'a|_]))).
test(l21_fmt_format_chars, (X = [a|X], format(X, [])), error(type_error(list, [a|_]))).
test(l21_fmt_format_culprit, (X = [a|X], catch(format(X, []), error(type_error(list, C), _), true), C == X), true).
test(l21_fmt_format_lasso, (X = [b|X], L = [a|X], catch(format(L, []), error(type_error(list, C), _), true), C == L), true).
test(l21_fmt3_cyclic, (X = [a|X], format(user_output, "~w", X)), error(type_error(list, [a|_]))).
test(l21_fmt_s_cyclic, (X = [0'a|X], format("~s", [X])), error(type_error(list, [0'a|_]))).
test(l21_fmt_s_culprit, (X = [0'a|X], catch(format("~s", [X]), error(type_error(list, C), _), true), C == X), true).
test(l21_fmt_cyclic_no_output, (X = [a|X], with_output_to(atom(A), catch(format("ab~w", X), _, true))), solutions([A = ''])).
% 循環が要素にあるだけなら、背骨は真のリストである（L3）。
test(l21_fmt_cyclic_element, (X = [a|X], with_output_to(atom(A), format("~w", [X]))), solutions([A = '[a|_]'])).
% レビュー M29。無限大と NaN の ~e と ~f は桁数にも標準ライブラリにもよらず、writeq と同じ字面を書き、0 で埋めない。
% 1100 桁を超える桁数は、以前は指数の位置を探して見つけられずにプロセスごと落ちた。
test(m29_fmt_e_inf, (X is inf, with_output_to(atom(A), format("~e", [X]))), solutions([A = '1.0Inf'])).
test(m29_fmt_f_inf, (X is inf, with_output_to(atom(A), format("~f", [X]))), solutions([A = '1.0Inf'])).
test(m29_fmt_e_neg_inf, (X is -inf, with_output_to(atom(A), format("~e", [X]))), solutions([A = '-1.0Inf'])).
test(m29_fmt_f_neg_inf, (X is -inf, with_output_to(atom(A), format("~f", [X]))), solutions([A = '-1.0Inf'])).
test(m29_fmt_e_nan, (X is nan, with_output_to(atom(A), format("~e", [X]))), solutions([A = '1.5NaN'])).
test(m29_fmt_f_nan, (X is nan, with_output_to(atom(A), format("~f", [X]))), solutions([A = '1.5NaN'])).
test(m29_fmt_e_signed_nan, (X is -(nan), with_output_to(atom(A), format("~e", [X]))), solutions([A = '1.5NaN'])).
test(m29_fmt_f_signed_nan, (X is -(nan), with_output_to(atom(A), format("~f", [X]))), solutions([A = '1.5NaN'])).
test(m29_fmt_zero_digits, (X is inf, with_output_to(atom(A), format("~0e ~0f", [X, X]))), solutions([A = '1.0Inf 1.0Inf'])).
test(m29_fmt_e_inf_many, (X is inf, with_output_to(atom(A), format("~1200e", [X]))), solutions([A = '1.0Inf'])).
test(m29_fmt_f_inf_many, (X is inf, with_output_to(atom(A), format("~1200f", [X]))), solutions([A = '1.0Inf'])).
test(m29_fmt_e_nan_many, (X is nan, with_output_to(atom(A), format("~1200e", [X]))), solutions([A = '1.5NaN'])).
test(m29_fmt_f_nan_many, (X is nan, with_output_to(atom(A), format("~1200f", [X]))), solutions([A = '1.5NaN'])).
test(m29_fmt_star_inf, (X is -inf, with_output_to(atom(A), format("~*e", [1200, X]))), solutions([A = '-1.0Inf'])).
test(m29_float_e_inf, (X is inf, '$format_float'(e, 1200, X, A)), solutions([A = '1.0Inf'])).
test(m29_float_f_nan, (X is -(nan), '$format_float'(f, 1200, X, A)), solutions([A = '1.5NaN'])).
% 有限の数は 1100 桁を超えても、指数の前か末尾に 0 を足す。期待する原子は桁を埋める ~`0t~N| で組む。
% 書いたときの sub_atom/5 は長い原子で候補をすべて数え上げて遅かったので使っていない（レビュー M32 で直した）。
test(m29_fmt_e_many_finite, (with_output_to(atom(Z), format("~`0t~1200|", [])), atom_concat('1.', Z, P), atom_concat(P, 'e+00', E), with_output_to(atom(A), format("~1200e", [1.0])), A == E), true).
test(m29_fmt_f_many_finite, (with_output_to(atom(Z), format("~`0t~1199|", [])), atom_concat('0.5', Z, E), with_output_to(atom(A), format("~1200f", [0.5])), A == E), true).
% フェーズ11のタスク8。~E も 1100 桁を超えると指数の前に 0 を足す。~g と ~G は末尾の 0 を落とす書式なので 0 を足さず、
% 桁の数が 1100 を超えても多倍長でも、double の 2 進展開を書き尽くした字面になる。
test(fmt_E_many_finite, (with_output_to(atom(Z), format("~`0t~1200|", [])), atom_concat('1.', Z, P), atom_concat(P, 'E+00', E), with_output_to(atom(A), format("~1200E", [1.0])), A == E), true).
test(fmt_g_many, (with_output_to(atom(A), format("~1200g", [0.5])), with_output_to(atom(B), format("~1200G", [0.1]))), solutions([(A = '0.5', B = '0.1000000000000000055511151231257827021181583404541015625')])).
test(fmt_g_digits_big, with_output_to(atom(A), format("~*g", [2^70, 0.5])), solutions([A = '0.5'])).
