test(atom_length_evening, atom_length('enchanted evening', N), solutions([N = 17])).
test(atom_length_evening_n, atom_length('enchanted evening', 17), true).
% 8.16.1.4。バックスラッシュと改行だけを消し、直後の空白は残す。
test(atom_length_continued, atom_length('enchanted\
 evening', N), solutions([N = 17])).
test(atom_length_empty, atom_length('', N), solutions([N = 0])).
test(atom_concat_hello, atom_concat('hello', ' world', A), solutions([A = 'hello world'])).
test(atom_concat_small, atom_concat(A, ' world', 'small world'), solutions([A = small])).
test(atom_concat_world, atom_concat('small', A, 'small world'), solutions([A = ' world'])).
test(atom_concat_split, atom_concat(A1, A2, hello), solutions([
    (A1 = '', A2 = hello),
    (A1 = h, A2 = ello),
    (A1 = he, A2 = llo),
    (A1 = hel, A2 = lo),
    (A1 = hell, A2 = o),
    (A1 = hello, A2 = '')
])).
test(atom_concat_open, atom_concat(A, B, C), error(instantiation_error)).
test(sub_atom_left, sub_atom(abracadabra, 0, 5, A, S), solutions([(A = 6, S = abrac)])).
test(sub_atom_right, sub_atom(abracadabra, B, 5, 0, S), solutions([(B = 6, S = dabra)])).
test(sub_atom_abra, sub_atom(abracadabra, B, 4, A, abra), solutions([
    (B = 0, A = 7),
    (B = 7, A = 0)
])).
test(sub_atom_ab, sub_atom(ab, B, L, A, S), solutions([
    (B = 0, L = 0, A = 2, S = ''),
    (B = 0, L = 1, A = 1, S = a),
    (B = 0, L = 2, A = 0, S = ab),
    (B = 1, L = 0, A = 1, S = ''),
    (B = 1, L = 1, A = 0, S = b),
    (B = 2, L = 0, A = 0, S = '')
])).
test(sub_atom_open, sub_atom(Atom, 0, 1, 0, S), error(instantiation_error)).
test(atom_chars_empty, atom_chars('', L), solutions([L = []])).
test(atom_chars_out, atom_chars(abc, L), solutions([L = [a, b, c]])).
test(atom_chars_in, atom_chars(X, [a, b, c]), solutions([X = abc])).
test(atom_chars_open, atom_chars(X, Y), error(instantiation_error)).
test(atom_codes_empty, atom_codes('', L), solutions([L = []])).
test(atom_codes_out, atom_codes(abc, L), solutions([L = [97, 98, 99]])).
test(atom_codes_in, atom_codes(X, [97, 98, 99]), solutions([X = abc])).
test(char_code_out, char_code(a, C), solutions([C = 97])).
test(char_code_in, char_code(C, 99), solutions([C = c])).
test(char_code_open, char_code(C, Code), error(instantiation_error)).
test(number_chars_int, number_chars(33, L), solutions([L = ['3', '3']])).
test(number_chars_in, number_chars(X, ['3', '3']), solutions([X = 33])).
test(number_chars_neg, number_chars(-25, L), solutions([L = ['-', '2', '5']])).
test(number_chars_neg_in, number_chars(X, ['-', '2', '5']), solutions([X = -25])).
test(number_chars_float, number_chars(33.0, L), solutions([L = ['3', '3', '.', '0']])).
test(number_chars_float_in, number_chars(X, ['3', '.', '3']), solutions([X = 3.3])).
test(number_codes_int, number_codes(33, L), solutions([L = [51, 51]])).
test(number_codes_in, number_codes(X, [51, 51]), solutions([X = 33])).
test(number_codes_neg, number_codes(X, [45, 50, 53]), solutions([X = -25])).
test(number_chars_letter, number_chars(1, [a]), error(syntax_error(_))).
test(number_chars_empty, number_chars(1, []), error(syntax_error(_))).
test(number_chars_leading_zero, number_chars(1, ['0', '1']), true).
test(number_chars_exponent, number_chars(1.0e9, ['1', '.', '0', 'E', '9']), true).
test(number_chars_space, number_chars(N, [' ', '1']), solutions([N = 1])).
% 0' の直後の空白はレイアウト文ではなく、文字コード 32 そのもの。
test(number_chars_char_code_space, number_chars(X, ['0', '''', ' ']), solutions([X = 32])).
test(number_chars_char_code_space_neg, number_chars(X, ['-', '0', '''', ' ']), solutions([X = -32])).
test(number_chars_newline, number_chars(N, ['\n', '1']), solutions([N = 1])).
test(number_chars_iso_layout, number_chars(A, ['\n', ' ', '3']), solutions([A = 3])).
test(number_codes_iso_layout, number_codes(A, [10, 32, 51]), solutions([A = 3])).
test(number_chars_comment, number_chars(A, ['/', '*', 'x', '*', '/', ' ', '4']), solutions([A = 4])).
test(number_chars_minus_space, number_chars(N, [-, ' ', '1']), error(syntax_error(_))).
test(number_chars_plus, number_chars(N, ['+', '1']), error(syntax_error(_))).
test(number_codes_letter, number_codes(1, [97]), error(syntax_error(_))).
% レビュー L22（iso-notes 6.4.5 の B）。数の字面を読む述語も 1.0Inf と 1.5NaN を読み、書いた字面と往復する。
test(l22_number_codes_inf, (number_codes(X, "1.0Inf"), float(X), X > 0, number_codes(X, C), atom_codes(A, C)), solutions([A = '1.0Inf'])).
test(l22_number_codes_neg_inf, (number_codes(X, " -1.0Inf"), float(X), X < 0), true).
test(l22_number_chars_nan, (number_chars(X, ['1', '.', '5', 'N', a, 'N']), \+ X =:= X, number_chars(X, C)), solutions([C = ['1', '.', '5', 'N', a, 'N']])).
test(l22_atom_number_neg_nan, (Y is -(nan), atom_number(A, Y), atom_number(A, Z), \+ Z =:= Z), solutions([A = '1.5NaN'])).
test(l22_number_codes_info, number_codes(_, "1.0Info"), error(syntax_error(_))).
test(number_codes_empty, number_codes(1, []), error(syntax_error(_))).
test(number_codes_leading_zero, number_codes(1, [48, 49]), true).
test(number_codes_space, number_codes(N, [32, 49]), solutions([N = 1])).
test(number_codes_char_code_space, number_codes(X, [48, 39, 32]), solutions([X = 32])).
test(number_codes_plus, number_codes(N, [43, 49]), error(syntax_error(_))).
% ADR 0007 の鍵（iso-notes 8.16 の決定 A）。字句の誤りは字句の理由、数でない並びと余りは unexpected_token。
test(number_key_letter, number_chars(N, [a]), error(syntax_error(unexpected_token))).
test(number_key_empty, number_chars(N, []), error(syntax_error(unexpected_token))).
test(number_key_trailing, number_chars(N, ['1', a]), error(syntax_error(unexpected_token))).
test(number_key_inner_space, number_chars(N, [-, ' ', '1']), error(syntax_error(unexpected_token))).
test(number_key_overflow, number_chars(N, ['9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9', '9']), error(syntax_error(integer_overflow))).
test(number_key_bad_escape, number_chars(N, ['0', '''', '\\', z]), error(syntax_error(bad_escape))).
test(number_key_open_comment, number_codes(N, [47, 42, 32, 49]), error(syntax_error(unterminated_comment))).
% atom_number/2（非 ISO、iso-notes 8.16 の決定 A）。規格の Examples は無いので、決定文の振る舞いを 1 行ずつ。
test(atom_number_to_number, atom_number('12', N), solutions([N = 12])).
test(atom_number_to_float, atom_number('1.5', N), solutions([N = 1.5])).
test(atom_number_to_negative, atom_number('-3', N), solutions([N = -3])).
test(atom_number_to_atom, atom_number(A, 12), solutions([A = '12'])).
test(atom_number_float_to_atom, atom_number(A, 1.5), solutions([A = '1.5'])).
test(atom_number_negative_to_atom, atom_number(A, -3), solutions([A = '-3'])).
test(atom_number_both_var, atom_number(_, _), error(instantiation_error)).
test(atom_number_not_number, atom_number(foo, _), error(syntax_error(unexpected_token))).
test(atom_number_match, atom_number('12', 12), true).
test(atom_number_mismatch, atom_number('12', 13), fail).
test(atom_number_type_atom, atom_number(12, _), error(type_error(atom, 12))).
test(atom_number_type_number, atom_number(_, foo), error(type_error(number, foo))).
test(sub_atom_int_before_sub, sub_atom(abc, foo, _, _, 1), error(type_error(integer, foo))).
% レビュー M2（iso-notes 6.4.5 の決定 A）。数の構文解析を共有するので、double に入らない浮動小数点数は字句の鍵になる。
test(m2_number_codes_big, number_codes(N, "1.0e400"), error(syntax_error(float_overflow))).
test(m2_number_chars_negative, number_chars(N, ['-', '1', '.', '0', e, '4', '0', '0']), error(syntax_error(float_overflow))).
test(m2_atom_number_big, atom_number('1.0e400', N), error(syntax_error(float_overflow))).
test(m2_number_codes_underflow, number_codes(N, "1.0e-400"), solutions([N = 0.0])).
% 溢れの向きは指数の符号でなく値の桁で決まる（M2 の敵対的レビュー A1/A2）。0 を 400 個並べた仮数部で確かめる。
m2_zero_codes(0, []) :- !.
m2_zero_codes(K, [0'0|T]) :- J is K - 1, m2_zero_codes(J, T).
m2_zeros(Z) :- m2_zero_codes(400, L), atom_codes(Z, L).
m2_number([A, B, C], N) :- atom_concat(A, B, AB), atom_concat(AB, C, T), atom_number(T, N).
test(m2_long_big_negative_exp, (m2_zeros(Z), m2_number(['1', Z, '.0e-10'], _)), error(syntax_error(float_overflow))).
test(m2_long_tiny_no_exp, (m2_zeros(Z), m2_number(['0.', Z, '1'], N), N == 0.0), true).
% レビュー M32。sub_atom/5 は束縛された引数が許す候補だけを試す。8.16.3.4 の Examples のうち上に無いもの、
% 決まった引数の組ごとの解とその順（Before の昇順、同じ Before では Length の昇順）、同じ変数を 2 つの引数に
% 置いたとき、最後の解が選択点を残さないこと、長い原子で答えがすぐ返ること。
test(m32_iso_acada, sub_atom(abracadabra, 3, L, 3, S), solutions([(L = 5, S = acada)])).
test(m32_iso_ab, sub_atom(abracadabra, B, 2, A, ab), solutions([(B = 0, A = 9), (B = 7, A = 2)])).
test(m32_iso_banana, sub_atom('Banana', 3, 2, _, S), solutions([S = an])).
test(m32_iso_charity, sub_atom(charity, _, 3, _, S), solutions([S = cha, S = har, S = ari, S = rit, S = ity])).
test(m32_before, sub_atom(abc, 1, L, A, S), solutions([
    (L = 0, A = 2, S = ''),
    (L = 1, A = 1, S = b),
    (L = 2, A = 0, S = bc)
])).
test(m32_length, sub_atom(abc, B, 2, A, S), solutions([(B = 0, A = 1, S = ab), (B = 1, A = 0, S = bc)])).
test(m32_after, sub_atom(abc, B, L, 1, S), solutions([
    (B = 0, L = 2, S = ab),
    (B = 1, L = 1, S = b),
    (B = 2, L = 0, S = '')
])).
test(m32_before_length, sub_atom(abc, 1, 1, A, S), solutions([(A = 1, S = b)])).
test(m32_before_after, sub_atom(abc, 1, L, 0, S), solutions([(L = 2, S = bc)])).
test(m32_length_after, sub_atom(abc, B, 1, 0, S), solutions([(B = 2, S = c)])).
test(m32_all_three, sub_atom(abc, 1, 1, 1, S), solutions([S = b])).
test(m32_all_three_sum, sub_atom(abc, 1, 1, 0, _), fail).
test(m32_before_long, sub_atom(abc, 4, _, _, _), fail).
test(m32_length_long, sub_atom(abc, _, 4, _, _), fail).
test(m32_after_long, sub_atom(abc, _, _, 4, _), fail).
test(m32_two_long, sub_atom(abc, 2, 2, _, _), fail).
test(m32_huge_before, sub_atom(abc, 9223372036854775807, _, _, _), fail).
test(m32_huge_pair, sub_atom(abc, _, 9223372036854775807, 9223372036854775807, _), fail).
test(m32_huge_sum, sub_atom(abc, 1, 9223372036854775807, _, _), fail).
test(m32_sub, sub_atom(abcabc, B, L, A, bc), solutions([(B = 1, L = 2, A = 3), (B = 4, L = 2, A = 0)])).
test(m32_sub_overlap, sub_atom(aaaa, B, _, _, aa), solutions([B = 0, B = 1, B = 2])).
test(m32_sub_restart, sub_atom(aabaabaaab, B, _, _, aab), solutions([B = 0, B = 3, B = 7])).
test(m32_sub_period, sub_atom(abababab, B, _, _, abab), solutions([B = 0, B = 2, B = 4])).
test(m32_sub_empty, sub_atom(ab, B, L, A, ''), solutions([
    (B = 0, L = 0, A = 2),
    (B = 1, L = 0, A = 1),
    (B = 2, L = 0, A = 0)
])).
test(m32_sub_before, sub_atom(abcabc, 3, L, A, abc), solutions([(L = 3, A = 0)])).
test(m32_sub_before_miss, sub_atom(abcabc, 1, _, _, abc), fail).
test(m32_sub_after, sub_atom(abcabc, B, L, 0, abc), solutions([(B = 3, L = 3)])).
test(m32_sub_length_miss, sub_atom(abcabc, _, 2, _, abc), fail).
test(m32_sub_long, sub_atom(ab, _, _, _, abc), fail).
test(m32_sub_whole, sub_atom(abc, B, L, A, abc), solutions([(B = 0, L = 3, A = 0)])).
test(m32_alias_before_length, sub_atom(abcd, X, X, A, S), solutions([
    (X = 0, A = 4, S = ''),
    (X = 1, A = 2, S = b),
    (X = 2, A = 0, S = cd)
])).
test(m32_alias_length_after, sub_atom(abcd, B, X, X, S), solutions([
    (B = 0, X = 2, S = ab),
    (B = 2, X = 1, S = c),
    (B = 4, X = 0, S = '')
])).
test(m32_alias_before_after, sub_atom(abab, X, 2, X, S), solutions([(X = 1, S = ba)])).
test(m32_alias_sub, sub_atom(xabxab, X, _, X, ab), fail).
test(m32_alias_sub_mid, sub_atom(xabx, X, _, X, ab), solutions([X = 1])).
test(m32_alias_before_after_free, sub_atom(abcd, X, L, X, S), solutions([
    (X = 0, L = 4, S = abcd),
    (X = 1, L = 2, S = bc),
    (X = 2, L = 0, S = '')
])).
test(m32_alias_length_after_odd, sub_atom(abcde, B, X, X, S), solutions([
    (B = 1, X = 2, S = bc),
    (B = 3, X = 1, S = d),
    (B = 5, X = 0, S = '')
])).
test(m32_alias_all_three, sub_atom(abcdef, X, X, X, S), solutions([(X = 2, S = cd)])).
test(m32_alias_all_three_miss, sub_atom(abcde, X, X, X, _), fail).
test(m32_alias_halved_before, sub_atom(abcde, 1, X, X, S), solutions([(X = 2, S = bc)])).
test(m32_alias_halved_odd, sub_atom(abcd, 1, X, X, _), fail).
test(m32_alias_halved_length, sub_atom(abcde, X, 1, X, S), solutions([(X = 2, S = c)])).
test(m32_alias_halved_after, sub_atom(abcde, X, X, 1, S), solutions([(X = 2, S = cd)])).
test(m32_alias_sub_half, sub_atom(aaaaaa, X, _, X, aa), solutions([X = 2])).
test(m32_alias_sub_length, sub_atom(xxab, X, X, A, ab), solutions([(X = 2, A = 0)])).
test(m32_alias_sub_length_miss, sub_atom(aabab, X, X, _, ab), fail).
test(m32_alias_sub_var_before, sub_atom(abc, S, _, _, S), fail).
test(m32_alias_sub_var_length, sub_atom(abc, _, S, _, S), fail).
test(m32_alias_sub_var_after, sub_atom(abc, _, _, S, S), fail).
% setup_call_cleanup/3 の後始末は、ゴールが選択点を残さずに成功したときだけ、その直後に走る。
m32_det(Goal) :- setup_call_cleanup(true, Goal, Det = yes), Det == yes.
test(m32_det_length_after, m32_det(sub_atom(abc, _, 1, 0, _)), true).
test(m32_det_before_length, m32_det(sub_atom(abc, 1, 1, _, _)), true).
test(m32_det_sub_before, m32_det(sub_atom(abcabc, 3, _, _, abc)), true).
test(m32_det_last_match, (m32_det(sub_atom(abcab, B, _, _, ab)), B == 3), true).
test(m32_det_last_before, (m32_det(sub_atom(abc, 1, L, _, _)), L == 2), true).
test(m32_det_last_length, (m32_det(sub_atom(abc, B, 2, _, _)), B == 1), true).
test(m32_det_last_after, (m32_det(sub_atom(abc, B, _, 1, _)), B == 2), true).
test(m32_det_last_all, (m32_det(sub_atom(ab, B, L, _, _)), B == 2, L == 0), true).
test(m32_det_alias_sub, m32_det(sub_atom(aaaa, X, 2, X, aa)), true).
test(m32_det_alias_last, (m32_det(sub_atom(abcd, X, X, _, _)), X == 2), true).
test(m32_det_alias_after_last, (m32_det(sub_atom(abcd, X, _, X, _)), X == 2), true).
test(m32_det_alias_length_last, (m32_det(sub_atom(abcde, B, X, X, _)), B == 5), true).
test(m32_nondet_first_match, (setup_call_cleanup(true, sub_atom(abcab, B, _, _, ab), Det = yes), B == 0, var(Det)), true).
% 2^K 個の a の原子。atom_concat で倍にするので、GC ストレスでも組むのは K 回で済む。
m32_as(0, a) :- !.
m32_as(K, A) :- J is K - 1, m32_as(J, H), atom_concat(H, H, A).
test(m32_long_end, (m32_as(12, A), sub_atom(A, 4095, 1, 0, S)), solutions([S = a])).
test(m32_long_length_after, (m32_as(12, A), sub_atom(A, B, 1, 0, _)), solutions([B = 4095])).
test(m32_long_match, (m32_as(12, A0), atom_concat(A0, b, A), sub_atom(A, B, _, _, b)), solutions([B = 4096])).
test(m32_long_absent, (m32_as(12, A), m32_as(11, P0), atom_concat(P0, b, P), \+ sub_atom(A, _, _, _, P)), true).
test(m32_long_alias_sub, (m32_as(12, A), m32_as(11, P), sub_atom(A, X, _, X, P)), solutions([X = 1024])).
test(m32_long_count, (m32_as(10, A), findall(B, sub_atom(A, B, _, _, a), Bs), length(Bs, N)), solutions([N = 1024])).
