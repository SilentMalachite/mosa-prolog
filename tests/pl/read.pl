test(read_atom, (open_string('atom.', S), read_term(S, T, []), T == atom), true).
test(read_int, (open_string('4.', S), read(S, 4)), true).
test(read_list, (open_string('[a, b].', S), read(S, [a, b])), true).
test(read_op, (open_string('1 + 2.', S), read(S, T), T == 1 + 2), true).
test(read_share, (open_string('f(A, A).', S), read(S, f(X, Y)), X == Y), true).
test(read_two, (open_string('a. b.', S), read(S, a), read(S, b)), true).
test(read_end_atom, (open_string('end_of_file. a.', S), read(S, end_of_file), read(S, a)), true).
test(read_eof, (open_string('', S), read(S, A), read(S, B), A == end_of_file, B == end_of_file), true).
test(read_eof_past, (open_string('', S), read(S, end_of_file), stream_property(S, end_of_stream(past))), true).
test(read_layout_eof, (open_string('  ', S), read(S, end_of_file)), true).
% 8.14.1.4 は終止符の直後の改行も残す。位置はドットの次で、次の read は改行を飛ばして b を読む。
test(read_position, (open_string([97, 46, 10, 98, 46], S), read(S, a), stream_property(S, position(stream_position(2, 1, 3))), read(S, b)), true).
test(read_keeps_layout, (open_string('term1. term2. ...', S), read(S, T), T == term1, get_char(S, C), C == ' '), true).
test(read_fail_consumes, (open_string('a. b.', S), (read(S, b) ; true), read(S, T), T == b), true).
test(read_names, (open_string('foo(A, B, A).', S), read_term(S, T, [variables(L), variable_names(N), singletons(G)]), T = foo(A, B, A), L = [A, B], N = ['A'=A, 'B'=B], G = ['B'=B]), true).
test(read_anon, (open_string('foo(_, A, _).', S), read_term(S, T, [variables(L), variable_names(N), singletons(G)]), T = foo(X, A, Y), X \== Y, L = [X, A, Y], N = ['A'=A], G = ['A'=A]), true).
% 真のリストが単一化に失敗するときは、項を消費してから失敗する。1 はリストではない。
test(vars_fail_consumes, (open_string('a. b.', S), (read_term(S, _, [variables([1])]) ; true), read(S, T), T == b), true).
% リストでない引数は読む前に domain_error。入力を置くのは、読む実装が標準入力で止まらないため。
test(vars_atom, (open_string('a.', S), read_term(S, _, [variables(a)])), error(domain_error(read_option, variables(a)))).
test(vars_improper, (open_string('a.', S), read_term(S, _, [variables([_|a])])), error(domain_error(read_option, variables([_|a])))).
test(names_atom, (open_string('a.', S), read_term(S, _, [variable_names(a)])), error(domain_error(read_option, variable_names(a)))).
test(names_improper, (open_string('a.', S), read_term(S, _, [variable_names([_|a])])), error(domain_error(read_option, variable_names([_|a])))).
test(singles_atom, (open_string('a.', S), read_term(S, _, [singletons(a)])), error(domain_error(read_option, singletons(a)))).
test(singles_improper, (open_string('a.', S), read_term(S, _, [singletons([_|a])])), error(domain_error(read_option, singletons([_|a])))).
test(vars_atom_keeps, (open_string('a.', S), catch(read_term(S, _, [variables(a)]), error(domain_error(read_option, variables(a)), _), true), read(S, a)), true).
test(quoted_true, (open_string('a.', S), read_term(S, a, [quoted(true)])), true).
test(quoted_false, (open_string('a.', S), read_term(S, a, [quoted(false)])), true).
test(quotes_codes, (open_string([34, 97, 98, 34, 46], S), read_term(S, T, []), T == [97, 98]), true).
test(quotes_chars, (set_prolog_flag(double_quotes, chars), open_string([34, 97, 98, 34, 46], S), read_term(S, T, []), set_prolog_flag(double_quotes, codes), T == [a, b]), true).
test(quotes_atom, (set_prolog_flag(double_quotes, atom), open_string([34, 97, 98, 34, 46], S), read_term(S, T, []), set_prolog_flag(double_quotes, codes), T == ab), true).
test(read_current, (open_string('a.', S), set_input(S), read(T), set_input(user_input), T == a), true).
test(read_term_current, (open_string('b.', S), set_input(S), read_term(T, []), set_input(user_input), T == b), true).
test(read_var, read(S, _), error(instantiation_error)).
test(read_term_var, read_term(S, _, []), error(instantiation_error)).
test(opt_before_domain, read_term(foo, _, [X]), error(instantiation_error)).
test(domain_before_type, read_term(foo, _, bar), error(domain_error(stream_or_alias, foo))).
test(read_domain, read(foo, _), error(domain_error(stream_or_alias, foo))).
test(type_list, read_term(user_input, _, bar), error(type_error(list, bar))).
test(opt_partial, read_term(_, [variables(L)|X]), error(instantiation_error)).
test(opt_bad, read_term(user_input, _, [foo]), error(domain_error(read_option, foo))).
test(opt_before_permit, read_term(user_output, _, [foo]), error(domain_error(read_option, foo))).
test(quoted_bad, read_term(user_input, _, [quoted(foo)]), error(domain_error(read_option, quoted(foo)))).
test(quoted_var, read_term(user_input, _, [quoted(Q)]), error(instantiation_error)).
test(read_output, read(user_output, _), error(permission_error(input, stream, user_output))).
test(read_term_output, read_term(user_output, _, []), error(permission_error(input, stream, user_output))).
test(read_missing, read('$stream'(99), _), error(existence_error(stream, '$stream'(99)))).
test(read_closed, (open_string('a.', S), close(S), read(S, _)), error(existence_error(stream, S))).
test(read_syntax, (open_string('foo)', S), read(S, _)), error(syntax_error('行 1 列 4: 予期しないトークン )'))).
test(read_incomplete, (open_string(foo, S), read(S, _)), error(syntax_error('行 1 列 4: 項が閉じていません 終端'))).
% 6.5 の quoted char に生の改行とタブは無い。このファイルにその字を書くと、読み込み自体が落ちる。
test(quoted_raw_nl, (open_string([39, 97, 10, 98, 39, 46], S), read(S, _)), error(syntax_error(_))).
test(quoted_raw_tab, (open_string([39, 97, 9, 98, 39, 46], S), read(S, _)), error(syntax_error(_))).
test(read_syntax_stream, (open_string('foo)', S), catch(read(S, _), error(_, context(_, stream(S, 1, 4, 3))), true)), true).
