test(write_kanji, (with_output_to(atom(A), write(東)), A == 東), true).
test(writeq_space, (with_output_to(atom(A), writeq('東 京')), atom_chars(A, ['\'', 東, ' ', 京, '\''])), true).
test(writeq_bare, (with_output_to(atom(A), writeq(東京)), A == 東京), true).
test(write_op, (with_output_to(atom(A), write(1 + 2)), A == '1+2'), true).
test(write_star_arrow, (with_output_to(atom(A), write(a('*->'(b,c)))), A == 'a((b*->c))'), true).
test(write_list, (with_output_to(atom(A), write([1, 2])), A == '[1,2]'), true).
test(write_term_list_iso, (with_output_to(atom(A), write_term([1,2,3], [])), A == '[1,2,3]'), true).
test(write_canonical_list_iso, (with_output_to(atom(A), write_canonical([1,2,3])), A == '\'.\'(1,\'.\'(2,\'.\'(3,[])))'), true).
test(write_bare_atom, (with_output_to(atom(A), write('1<2')), A == '1<2'), true).
test(writeq_atom, (with_output_to(atom(A), writeq('1<2')), A == '''1<2'''), true).
test(writeq_hex_2, (with_output_to(atom(A), writeq('\x2\')), A == '\'\\x2\\\''), true).
test(writeq_hex_11, (with_output_to(atom(A), writeq('\x11\')), A == '\'\\x11\\\''), true).
test(writeq_backtick, (with_output_to(atom(A), writeq('`')), A == '\'`\''), true).
test(write_term_quote, (with_output_to(atom(A), write_term('1<2', [quoted(true)])), A == '''1<2'''), true).
test(write_term_plain, (with_output_to(atom(A), write_term('1<2', [])), A == '1<2'), true).
test(write_term_plus, (with_output_to(atom(A), write_term(+1, [])), A == '+1'), true).
test(writeq_minus_atom, (with_output_to(atom(A), writeq(-(-))), A == '- (-)'), true).
test(write_term_minus_one, (with_output_to(atom(A), write_term(-1, [])), A == '-1'), true).
test(write_term_block_open, (with_output_to(atom(A), write_term('/*', [quoted(true)])), A == '\'/*\''), true).
test(write_term_block_close, (with_output_to(atom(A), write_term('*/', [quoted(true)])), A == '*/'), true).
test(writeq_dot, (with_output_to(atom(A), writeq('.')), A == '\'.\''), true).
test(write_dot, (with_output_to(atom(A), write('.')), A == '.'), true).
test(canonical_op, (with_output_to(atom(A), write_canonical(1 + 2)), A == '+(1,2)'), true).
test(canonical_list, (with_output_to(atom(A), write_canonical([1, 2, 3])), A == '''.''(1,''.''(2,''.''(3,[])))'), true).
test(canonical_brace, (with_output_to(atom(A), write_canonical({a})), A == '{}(a)'), true).
test(numbervar_a, (with_output_to(atom(A), write_term('$VAR'(0), [numbervars(true)])), A == 'A'), true).
test(numbervar_a1, (with_output_to(atom(A), write_term('$VAR'(26), [numbervars(true)])), A == 'A1'), true).
test(numbervar_z1, (with_output_to(atom(A), write_term('$VAR'(51), [numbervars(true)])), A == 'Z1'), true).
test(numbervar_neg, (with_output_to(atom(A), write_term('$VAR'(-1), [numbervars(true), quoted(true)])), A == '''$VAR''(-1)'), true).
test(write_numbervar, (with_output_to(atom(A), write('$VAR'(0))), A == 'A'), true).
test(writeq_numbervar, (with_output_to(atom(A), writeq('$VAR'(0))), A == 'A'), true).
test(canonical_numbervar, (with_output_to(atom(A), write_canonical('$VAR'(0))), A == '''$VAR''(0)'), true).
test(var_names, (with_output_to(atom(A), write_term(f(X, X), [variable_names(['A'=X])])), A == 'f(A,A)'), true).
test(var_names_quoted, (with_output_to(atom(A), write_term(X, [quoted(true), variable_names(['A'=X])])), A == 'A'), true).
test(var_names_bound, (X = 1, with_output_to(atom(A), write_term(X, [quoted(true), variable_names(['A'=X])])), A == '1'), true).
% iso-notes 7.10.5 a の決定 B。1 度だけ出る変数も _G と番号で書く。
test(write_fresh, (with_output_to(atom(A), write(X)), A == '_G1'), true).
test(write_two_fresh, (with_output_to(atom(A), write(f(X, Y))), A == 'f(_G1,_G2)'), true).
test(write_named_rest, (with_output_to(atom(A), write_term(f(X, Y), [variable_names(['X' = X])])), A == 'f(X,_G1)'), true).
test(write_share, (with_output_to(atom(A), write(f(X, X))), A == 'f(_G1,_G1)'), true).
test(write_cycle, (X = f(X), with_output_to(atom(A), write(X)), A == 'f(_)'), true).
test(write_stream, (with_output_to(atom(A), (current_output(S), write(S, 東))), A == 東), true).
test(writeq_stream, (with_output_to(atom(A), (current_output(S), writeq(S, '東 京'))), atom_chars(A, ['\'', 東, ' ', 京, '\''])), true).
test(canonical_stream, (with_output_to(atom(A), (current_output(S), write_canonical(S, [1]))), A == '''.''(1,[])'), true).
test(write_term_stream, (with_output_to(atom(A), (current_output(S), write_term(S, '1<2', [quoted(true)]))), A == '''1<2'''), true).
test(print_same, (with_output_to(atom(A), print('東 京')), with_output_to(atom(B), write('東 京')), A == B, A == '東 京'), true).
test(print_stream, (with_output_to(atom(A), (current_output(S), print(S, 東))), A == 東), true).
test(write_position, (with_output_to(atom(A), (current_output(S), write(S, ab), stream_property(S, position(P)), P == stream_position(2, 1, 3))), A == ab), true).
test(capture_codes, (with_output_to(codes(C), write(ab)), C == [97, 98]), true).
test(capture_empty, (with_output_to(atom(A), true), A == ''), true).
test(capture_codes_empty, (with_output_to(codes(C), true), C == []), true).
test(capture_restores, (with_output_to(atom(_), write(a)), current_output(user_output)), true).
test(capture_fail_restores, (with_output_to(atom(_), fail) ; current_output(user_output)), true).
test(capture_throw_restores, catch(with_output_to(atom(_), throw(x)), x, current_output(user_output)), true).
test(capture_once, (with_output_to(atom(A), (write(a) ; write(b))), A == a), true).
test(capture_keeps_bind, (with_output_to(atom(A), (X = a, write(X))), X == a, A == a), true).
test(capture_fail_unbinds, ((with_output_to(atom(_), (X = a, fail)) ; true), var(X)), true).
test(capture_inner_set, (with_output_to(atom(A), (write(a), set_output(user_error))), current_output(user_output), A == a), true).
test(capture_closes, (with_output_to(atom(A), (current_output(S), write(a))), open_string(b, T), S \== T, A == a, stream_property(S, mode(write))), error(existence_error(stream, S))).
test(capture_fail_closes, (findall(S, stream_property(S, mode(write)), Before), (with_output_to(atom(_), fail) ; true), findall(S, stream_property(S, mode(write)), After), Before == After), true).
test(capture_throw_closes, (catch(with_output_to(codes(_), (current_output(S), throw(kept(S)))), kept(S), true), write(S, c)), error(existence_error(stream, S))).
test(capture_leaves_output, (with_output_to(user_output, true), stream_property(user_output, mode(append))), true).
test(write_var, write(S, a), error(instantiation_error)).
test(write_term_var, write_term(S, a, []), error(instantiation_error)).
test(opt_before_domain, write_term(foo, a, [X]), error(instantiation_error)).
test(domain_before_type, write_term(foo, a, bar), error(domain_error(stream_or_alias, foo))).
test(type_list, write_term(user_output, a, bar), error(type_error(list, bar))).
test(opt_partial, write_term(a, [quoted(true)|X]), error(instantiation_error)).
test(opt_bad, write_term(user_output, a, [foo]), error(domain_error(write_option, foo))).
test(opt_before_permit, write_term(user_input, a, [foo]), error(domain_error(write_option, foo))).
test(quoted_bad, write_term(a, [quoted(foo)]), error(domain_error(write_option, quoted(foo)))).
test(quoted_var, write_term(a, [quoted(Q)]), error(instantiation_error)).
% iso-notes 8.14 Write options の決定 B（2026-10-28）。繰り返した選択肢は後の方が勝つ。
test(opt_repeat_last, (with_output_to(atom(A), write_term('A', [quoted(true), quoted(false)])), A == 'A'), true).
test(opt_repeat_quoted, (with_output_to(atom(A), write_term('A', [quoted(false), quoted(true)])), A == '\'A\''), true).
test(opt_repeat_numbervars, (with_output_to(atom(A), write_term('$VAR'(0), [numbervars(true), numbervars(false)])), A == '$VAR(0)'), true).
test(opt_repeat_numbervars_on, (with_output_to(atom(A), write_term('$VAR'(0), [numbervars(false), numbervars(true)])), A == 'A'), true).
test(opt_repeat_names, (with_output_to(atom(A), write_term(V, [variable_names(['Ignored'=V]), variable_names(['Used'=V])])), A == 'Used'), true).
test(opt_repeat_still_checked, write_term(a, [quoted(true), quoted(foo)]), error(domain_error(write_option, quoted(foo)))).
test(opt_repeat_names_checked, write_term(a, [variable_names([]), variable_names(a)]), error(domain_error(write_option, variable_names(a)))).
test(names_bad, write_term(X, [variable_names([a=X])]), error(domain_error(write_option, a=X))).
test(write_input, write(user_input, a), error(permission_error(output, stream, user_input))).
test(write_term_input, write_term(user_input, a, []), error(permission_error(output, stream, user_input))).
test(write_missing, write('$stream'(99), a), error(existence_error(stream, '$stream'(99)))).
test(capture_var, with_output_to(S, true), error(instantiation_error)).
test(capture_goal_var, with_output_to(atom(_), G), error(instantiation_error)).
test(capture_goal_before_domain, with_output_to(no_such, G), error(instantiation_error)).
test(capture_goal_type, with_output_to(atom(_), 1), error(type_error(callable, 1))).
test(capture_alias, with_output_to(no_such, true), error(domain_error(stream_or_alias, no_such))).
test(capture_input, with_output_to(user_input, true), error(permission_error(output, stream, user_input))).
test(capture_atom_type, with_output_to(atom(1), true), error(type_error(atom, 1))).
% 6.3.4.1: `- 1` も負の数と読まれる。-(1) は数を括弧で包んで前置演算子のまま読み戻す。
test(writeq_minus_int, (open_string('-(1). ', S), read(S, T), with_output_to(atom(A), writeq(T)), A == '- (1)'), true).
test(writeq_minus_float, (open_string('-(1.0). ', S), read(S, T), with_output_to(atom(A), writeq(T)), A == '- (1.0)'), true).
test(writeq_minus_neg, (open_string('-(-1). ', S), read(S, T), with_output_to(atom(A), writeq(T)), A == '- -1'), true).
test(writeq_minus_name, (with_output_to(atom(A), writeq(-(a))), A == '-a'), true).
% 8.14.2.3 の選択肢の検査。部分リストと quoted の変数は opt_partial と quoted_var が見る。
test(wt_opt_var, write_term(a, [_]), error(instantiation_error)).
test(wt_opt_quoted_fail, write_term(a, [quoted(fail)]), error(domain_error(write_option, quoted(fail)))).
test(wt_opt_vn_var, write_term(a, [variable_names(_)]), error(instantiation_error)).
test(wt_opt_vn_bad, write_term(a, [variable_names(a)]), error(domain_error(write_option, variable_names(a)))).
test(wt_opt_vn_tail, write_term(a, [variable_names(['A'=_|a])]), error(domain_error(write_option, variable_names(['A'=_|a])))).
test(wt_opt_vn_pair_var, write_term(a, [variable_names([_='A'])]), error(instantiation_error)).
test(wt_opt_ok, with_output_to(atom(_), write_term(a, [quoted(false), numbervars(true), variable_names(['Used'=_])])), true).
% 後ろの選択肢が誤りでも、書き出しは一文字も始めない。
test(wt_opt_no_partial, (with_output_to(atom(A), catch(write_term(f(a, b), [quoted(true), bogus]), _, true)), A == ''), true).
% 7.10.5、6.3.4.2: 演算子のアトムを演算子の被演算子にするときは括弧に入れる。引数（6.3.3.1）とリストの要素には付けない。
test(writeq_op_atom_left, (with_output_to(atom(A), writeq((-)/1)), A == '(-)/1'), true).
test(writeq_op_atom_neck, (with_output_to(atom(A), writeq((:-)/2)), A == '(:-)/2'), true).
test(writeq_op_atom_right, (with_output_to(atom(A), writeq(1-(-))), A == '1-(-)'), true).
test(writeq_op_atom_word, (with_output_to(atom(A), writeq(a=(is))), A == 'a=(is)'), true).
test(writeq_op_atom_both, (with_output_to(atom(A), writeq((-)=(+))), A == '(-)=(+)'), true).
test(writeq_op_atom_arg, (with_output_to(atom(A), writeq(f(:-))), A == 'f(:-)'), true).
test(writeq_op_atom_list, (with_output_to(atom(A), writeq([-, :-])), A == '[-,:-]'), true).
test(writeq_prefix_prefix, (with_output_to(atom(A), writeq(- - a)), A == '- -a'), true).
% 書いた字面を読み戻すと同じ項になる（7.10.5 の往復）。
test(writeq_round_left, (T = (-)/1, with_output_to(atom(A), writeq(T)), atom_concat(A, ' .', B), open_string(B, S), read(S, R), R == T), true).
test(writeq_round_neck, (T = (:-)/2, with_output_to(atom(A), writeq(T)), atom_concat(A, ' .', B), open_string(B, S), read(S, R), R == T), true).
test(writeq_round_right, (T = 1-(-), with_output_to(atom(A), writeq(T)), atom_concat(A, ' .', B), open_string(B, S), read(S, R), R == T), true).
test(writeq_round_word, (T = (a=(is)), with_output_to(atom(A), writeq(T)), atom_concat(A, ' .', B), open_string(B, S), read(S, R), R == T), true).
test(writeq_round_arg, (T = f(:-), with_output_to(atom(A), writeq(T)), atom_concat(A, ' .', B), open_string(B, S), read(S, R), R == T), true).
test(writeq_round_list, (T = [-, :-], with_output_to(atom(A), writeq(T)), atom_concat(A, ' .', B), open_string(B, S), read(S, R), R == T), true).
test(writeq_round_prefix, (T = (- - a), with_output_to(atom(A), writeq(T)), atom_concat(A, ' .', B), open_string(B, S), read(S, R), R == T), true).
