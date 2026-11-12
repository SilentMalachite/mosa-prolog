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
% 6.4.8: 終止符の後ろは空白か % である。% の注釈は次の read が飛ばす。
test(read_end_percent, (open_string('a.% c\n', S), read(S, T), read(S, U), T == a, U == end_of_file), true).
test(read_end_percent_eof, (open_string('a.%', S), read(S, T), T == a), true).
test(read_univ_percent, (open_string('X =..% c\n Y.', S), read(S, T), T = (_ =.. _)), true).
test(read_char_dot_percent, (open_string('0\'.%c\n', S), read(S, _)), error(syntax_error(_))).
test(read_end_percent_next, (open_string('a.%c\nb.', S), read(S, a), read(S, T), T == b), true).
end_percent_fact.% この節は終止符のすぐ後ろに % を置く。読めなければファイル全体が読み込めない。
test(read_end_percent_clause, end_percent_fact, true).
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
% ADR 0007。Formal は言語に依らない鍵。行、列、問題のトークンは Context の syntax(stream(...), Token) に置く。
test(read_syntax, (open_string('foo)', S), read(S, _)), error(syntax_error(unexpected_token))).
test(read_incomplete, (open_string(foo, S), read(S, _)), error(syntax_error(incomplete_term))).
% 6.4: , と | は区切りの字句で項にならない。| は演算子に定めたときだけアトム '|' になる（6.3.4.3、Cor.3）。
test(read_bare_comma, (open_string('f((,)).', S), read(S, _)), error(syntax_error(_))).
test(read_comma_functional, (open_string(',(a, b).', S), read(S, _)), error(syntax_error(_))).
test(read_bare_bar, (open_string('f((|)).', S), read(S, _)), error(syntax_error(_))).
test(read_bar_functional, (open_string('|(a, b).', S), read(S, _)), error(syntax_error(_))).
test(read_bar_operator_atom, (op(1105, xfy, '|'), open_string('f((|)).', S), read(S, T), op(0, xfy, '|'), T == f('|')), true).
test(read_quoted_comma_bar, (open_string('f(\',\', \'|\').', S), read(S, T), T == f(',', '|')), true).
% ADR 0011 改訂 4。枠のスタックで読んでも、深い入れ子の中の誤りは浅いときと同じ Formal になる。
test(read_error_deep_token, (open_string('f(g(h(i(j(,))))).', S), read(S, _)), error(syntax_error(_))).
test(read_error_deep_unclosed, (open_string('f(g(h(i(j(a)))).', S), read(S, _)), error(syntax_error(_))).
% 演算子の枠を積んだまま構文エラーになる（上の行は前置の枠を 4 つ積んだ所の f( の直後の `,`、下の行は中置の枠を
% 4 つ積んだ所の `)`）。reader は終止符まで読み飛ばし、同じストリームの次の項が読める。
test(read_error_prefix_frames_recover, (open_string('- - - - f(,). a.', S), catch(read(S, _), error(syntax_error(_), _), true), read(S, Y), Y == a), true).
test(read_error_xfy_frames_recover, (open_string('1:2:3:4: ). a.', S), catch(read(S, _), error(syntax_error(_), _), true), read(S, Y), Y == a), true).
% 6.5 の quoted char に生の改行とタブは無い。このファイルにその字を書くと、読み込み自体が落ちる。
test(quoted_raw_nl, (open_string([39, 97, 10, 98, 39, 46], S), read(S, _)), error(syntax_error(_))).
test(quoted_raw_tab, (open_string([39, 97, 9, 98, 39, 46], S), read(S, _)), error(syntax_error(_))).
test(read_syntax_stream, (open_string('foo)', S), catch(read(S, _), error(_, context(_, syntax(stream(S, 1, 4, 3), ')'))), true)), true).
test(read_syntax_end_token, (open_string(foo, S), catch(read(S, _), error(_, context(_, syntax(_, T))), true)), solutions([T = ''])).
test(quoted_raw_tab_token, (open_string([39, 97, 9, 98, 39, 46], S), catch(read(S, _), error(syntax_error(K), context(_, syntax(_, T))), true), K == illegal_character, T == '\t'), true).
% 空白の無い []( と {}( だけが関数表記。空白のあとの ( は括弧である。
test(read_nil_call, (open_string('[](2,3).', S), read(S, T), functor(T, [], 2)), true).
test(read_curly_call, (open_string('{}(2,3).', S), read(S, T), functor(T, {}, 2)), true).
test(read_nil_bare, (open_string('[].', S), read(S, T), T == []), true).
test(read_nil_space, (open_string('[] (2,3).', S), read(S, _)), error(syntax_error(_))).
test(read_curly_space, (open_string('{} (2,3).', S), read(S, _)), error(syntax_error(_))).
% 頭も尾も無い | は構文エラー。尾が無い [1|] も同じ。尾がある [1|2] は項のまま。
test(read_bar_none, (open_string('[|].', S), read(S, _)), error(syntax_error(_))).
test(read_bar_no_tail, (open_string('[1|].', S), read(S, _)), error(syntax_error(_))).
test(read_bar_pair, (open_string('[1|2].', S), read(S, T), T == [1|2]), true).
% \e \d \s は名前のエスケープ。\n と \t、および 0' の直後の生の空白とは別である。
test(read_escape_e, (open_string('0\'\\e.', S), read(S, T), T == 27), true).
test(read_escape_d, (open_string('0\'\\d.', S), read(S, T), T == 127), true).
test(read_escape_s, (open_string('0\'\\s.', S), read(S, T), T == 32), true).
% 0' の文字が引用符のときは二つ書いて 1 文字にする。値は 39。
test(read_quote_code, (open_string('0\'\'\'.', S), read(S, T), T == 39), true).
% 引用符が一つの 0''. も 39。次が引用符のときだけ二つ消費する。
test(read_quote_code_one, (open_string('0\'\'.', S), read(S, T), T == 39), true).
% 槽の先頭のコンマは区切りであり、引用しない原子にはならない。
test(read_empty_arg, (open_string('f(,,a).', S), read(S, _)), error(syntax_error(_))).
test(read_empty_elem, (open_string('[a,,|v].', S), read(S, _)), error(syntax_error(_))).
test(read_args_pair, (open_string('f(a,b).', S), read(S, T), T == f(a,b)), true).
% | の直後の solo コンマは尾の項ではない。引用した ',' は原子のまま。
test(read_bar_comma, (open_string('[a,b|,].', S), read(S, _)), error(syntax_error(_))).
test(read_bar_quoted_comma, (open_string('[a,b|\',\'].', S), read(S, T), T == [a,b|',']), true).
% 6.3.4.1: 名前 - の次の数は、間に空白があっても負の数。-(1) と関数表記で書けば複合項である。
test(read_neg_int, (open_string('-1. ', S), read(S, T), integer(T), T + 1 =:= 0), true).
test(read_neg_float, (open_string('-1.5. ', S), read(S, T), float(T), T + 1.5 =:= 0), true).
test(read_neg_layout, (open_string('- 1. ', S), read(S, T), integer(T), T + 1 =:= 0), true).
test(read_neg_prefix_paren, (open_string('- (1). ', S), read(S, T), T = -(A), A == 1), true).
test(read_neg_paren, (open_string('-(1). ', S), read(S, T), T = -(A), A == 1), true).
test(read_neg_twice, (open_string('- -1. ', S), read(S, T), T = -(A), integer(A), A + 1 =:= 0), true).
test(read_neg_infix, (open_string('a-1. ', S), read(S, T), T = -(a, B), B == 1), true).
test(read_neg_arg, (open_string('f(-1). ', S), read(S, T), T = f(A), integer(A)), true).
test(src_neg_layout, (X = - 1, integer(X)), true).
test(src_neg_paren, (X = -(1), compound(X)), true).
test(src_neg_int, (X = -1, integer(X)), true).
% レビュー M1。6.3.4.1: - の直後の数は負の定数なので、-9223372036854775808 は min_integer として読める。
% フェーズ10（ADR 0025）から int64 に入らない整数の字面は Big として読むので、- の付かない 2^63 も、絶対値が
% 2^63 を越える負の数も値になる。writeq した min_integer は読み戻せる（7.10.5）。
m1_read(Text, T) :- open_string(Text, S), read(S, T).
m1_min(M) :- current_prolog_flag(min_integer, M).
m1_back(T) :- with_output_to(atom(A), (writeq(T), write('.'))), m1_read(A, U), U == T.
m1_fact(-9223372036854775808).
m1_head(-9223372036854775808, min).
test(m1_read_min, (m1_read('-9223372036854775808.', X), m1_min(M), X == M), true).
test(m1_read_layout, (m1_read('- 9223372036854775808.', X), m1_min(M), X == M), true).
test(m1_read_comment, (m1_read('- /* c */9223372036854775808.', X), m1_min(M), X == M), true).
test(m1_read_zeros, (m1_read('-0009223372036854775808.', X), m1_min(M), X == M), true).
test(m1_read_underscore, (m1_read('-9_223_372_036_854_775_808.', X), m1_min(M), X == M), true).
test(m1_read_hex, (m1_read('-0x8000000000000000.', X), m1_min(M), X == M), true).
test(m1_read_octal, (m1_read('-0o1000000000000000000000.', X), m1_min(M), X == M), true).
test(m1_read_binary, (m1_read('-0b1000000000000000000000000000000000000000000000000000000000000000.', X), m1_min(M), X == M), true).
test(m1_read_in_term, (m1_read('f(-9223372036854775808, [-9223372036854775808|T]).', X), m1_min(M), X = f(A, [B|T]), A == M, B == M, var(T)), true).
test(m1_read_args, (m1_read('- 9223372036854775808 - -9223372036854775808.', X), m1_min(M), X = (A - B), A == M, B == M), true).
test(m1_read_positive, m1_read('9223372036854775808.', X), solutions([X = 9223372036854775808])).
test(m1_read_positive_hex, m1_read('0x8000000000000000.', X), solutions([X = 9223372036854775808])).
test(m1_read_below, m1_read('-9223372036854775809.', X), solutions([X = -9223372036854775809])).
test(m1_read_below_hex, m1_read('-0x8000000000000001.', X), solutions([X = -9223372036854775809])).
test(m1_read_far_below, m1_read('-92233720368547758080.', X), solutions([X = -92233720368547758080])).
test(m1_read_paren, m1_read('-(9223372036854775808).', X), solutions([X = -(9223372036854775808)])).
test(m1_read_infix, m1_read('a - 9223372036854775808.', X), solutions([X = a - 9223372036854775808])).
test(m1_read_arg, m1_read('f(9223372036854775808).', X), solutions([X = f(9223372036854775808)])).
test(m1_read_prefix_op, m1_read('\\+ 9223372036854775808.', X), solutions([X = (\+ 9223372036854775808)])).
test(m1_read_float_kept, (m1_read('-9223372036854775808.0.', X), float(X), X =:= -9223372036854775808.0), true).
test(m1_read_max_kept, (m1_read('-9223372036854775807.', X), current_prolog_flag(max_integer, Max), X =:= -Max), true).
test(m1_read_next, (open_string('-9223372036854775808. a.', S), read(S, X), read(S, Y), m1_min(M), X == M, Y == a), true).
test(m1_read_big_next, (open_string('9223372036854775808. a.', S), read(S, X), read(S, Y), X == 9223372036854775808, Y == a), true).
test(m1_back_min, (m1_min(M), m1_back(M)), true).
test(m1_back_minus_min, (m1_min(M), m1_back(-(M))), true).
test(m1_back_sub, (m1_min(M), m1_back(1 - M)), true).
test(m1_back_list, (m1_min(M), m1_back([M, f(M), - M])), true).
test(m1_source_fact, (m1_fact(X), m1_min(M), X == M), true).
test(m1_source_head, (m1_min(M), m1_head(M, R), R == min), true).
test(m1_source_is, (X is -9223372036854775808, m1_min(M), X == M), true).
test(m1_source_cmp, (m1_min(M), M =:= -9223372036854775808), true).
test(m1_number_codes, (number_codes(X, "-9223372036854775808"), m1_min(M), X == M), true).
test(m1_number_codes_hex, (number_codes(X, "-0x8000000000000000"), m1_min(M), X == M), true).
test(m1_atom_number_layout, (atom_number(' -9223372036854775808', X), m1_min(M), X == M), true).
test(m1_number_codes_positive, number_codes(X, "9223372036854775808"), solutions([X = 9223372036854775808])).
test(m1_number_codes_positive_hex, number_codes(X, "0x8000000000000000"), solutions([X = 9223372036854775808])).
test(m1_number_codes_plus, number_codes(_, "+9223372036854775808"), error(syntax_error(_))).
test(m1_number_codes_below, number_codes(X, "-9223372036854775809"), solutions([X = -9223372036854775809])).
test(m1_read_big_arg_next, (open_string('f(9223372036854775808, x). a.', S), read(S, X), read(S, Y), X == f(9223372036854775808, x), Y == a), true).
test(m1_read_big_paren_next, (open_string('-(0x8000000000000000). a.', S), read(S, X), read(S, Y), X == -(9223372036854775808), Y == a), true).
test(m1_read_big_last, (open_string('9223372036854775808.', S), read(S, X), read(S, Y), X == 9223372036854775808, Y == end_of_file), true).
% オペランドの後ろの数は、int64 に入る 'a 1.' と同じく unexpected_token になる。
test(m1_read_after_operand, m1_read('a 9223372036854775808.', _), error(syntax_error(unexpected_token))).
test(m1_read_after_operand_list, m1_read('[a 0x8000000000000000].', _), error(syntax_error(unexpected_token))).
test(m1_read_after_operand_recovers, (open_string('f(a 9223372036854775808). b.', S), catch(read(S, _), error(syntax_error(_), _), true), read(S, Y), Y == b), true).
% 1025 番目の引数が Big でも、ほかの整数と同じく max_arity になる（フェーズ10で integer_overflow は無くなった）。
m1_wide(0, Last, A) :- !, atom_concat(Last, ').', A).
m1_wide(N, Last, A) :- M is N - 1, m1_wide(M, Last, B), atom_concat('0,', B, A).
m1_wide_read(Last) :- m1_wide(1024, Last, Body), atom_concat('f(', Body, Text), m1_read(Text, _).
test(m1_read_wide_min, m1_wide_read('9223372036854775808'), error(syntax_error(max_arity))).
test(m1_read_wide_beyond, m1_wide_read('9223372036854775809'), error(syntax_error(max_arity))).
test(m1_read_wide_plain, m1_wide_read('1'), error(syntax_error(max_arity))).
% レビュー M2（持ち主は A を選んだ、iso-notes 6.4.5）。double に入らない浮動小数点数は inf.0 にせず、整数の溢れと
% 同じく syntax_error(float_overflow) とし、read/1 は終止符まで読み飛ばす。下位の溢れは 0.0 のままである。
test(m2_read_big, m1_read('1.0e400.', _), error(syntax_error(float_overflow))).
test(m2_read_just_above, m1_read('1.8e308.', _), error(syntax_error(float_overflow))).
test(m2_read_negative, m1_read('-1.0e400.', _), error(syntax_error(float_overflow))).
test(m2_read_in_term, m1_read('f(a, [1.0e400]).', _), error(syntax_error(float_overflow))).
test(m2_read_is, m1_read('X is 1.0e400 * 1.', _), error(syntax_error(float_overflow))).
test(m2_read_recovers, (open_string('X = 1.0e400. a.', S), catch(read(S, _), error(syntax_error(_), _), true), read(S, Y), Y == a), true).
test(m2_read_max_kept, (m1_read('1.7976931348623157e308.', X), X =:= 1.7976931348623157e308), true).
test(m2_read_underflow_kept, (m1_read('1.0e-400.', X), X == 0.0), true).
test(m2_read_subnormal_kept, (m1_read('1.0e-310.', X), X > 0.0, X < 1.0e-300), true).
% レビュー L22（持ち主は B を選んだ、iso-notes 6.4.5）。SWI-Prolog と ECLiPSe の 1.0Inf と 1.5NaN を数として読む。
% Inf の前はどの桁でもよく、NaN の前は 1.xxx だけ。名前が続く綴りや指数の付いた綴りは今までどおり構文の誤り。
% 1.0Inf は値が溢れた綴りではないので、float_overflow が error でも M2 の誤りにしない。
test(l22_read_inf, (m1_read('1.0Inf.', X), float(X), X > 1.7976931348623157e308), true).
test(l22_read_neg_inf, (m1_read('-1.0Inf.', X), float(X), X < -1.7976931348623157e308), true).
test(l22_read_inf_digits, (m1_read('[0.0Inf, 12.5Inf, 1_0.0Inf].', L), L = [A, B, C], A == B, B == C, A > 0), true).
test(l22_read_nan, (m1_read('1.5NaN.', X), float(X), \+ X =:= X), true).
test(l22_read_nan_mantissa, (m1_read('[1.0NaN, 1.999NaN].', L), L = [A, B], \+ A =:= A, \+ B =:= B), true).
test(l22_read_neg_nan, (m1_read('-1.5NaN.', X), float(X), \+ X =:= X), true).
test(l22_read_nan_two, m1_read('2.0NaN.', _), error(syntax_error(_))).
test(l22_read_nan_half, m1_read('0.5NaN.', _), error(syntax_error(_))).
% 仮数の範囲は丸めた double でなく桁で決める（L22 の Codex レビュー）。
test(l22_read_nan_ten, m1_read('1_0.5NaN.', _), error(syntax_error(_))).
test(l22_read_nan_near_two, (m1_read('[1.99999999999999999NaN, 01.5NaN].', L), L = [A, B], \+ A =:= A, \+ B =:= B), true).
test(l22_read_nan_near_one, m1_read('0.99999999999999999NaN.', _), error(syntax_error(_))).
test(l22_read_info, m1_read('1.0Info.', _), error(syntax_error(_))).
test(l22_read_nan_longer, m1_read('1.5NaNa.', _), error(syntax_error(_))).
test(l22_read_inf_digit, m1_read('1.0Inf1.', _), error(syntax_error(_))).
test(l22_read_inf_underline, m1_read('1.0Inf_.', _), error(syntax_error(_))).
test(l22_read_exp_inf, m1_read('1.0e5Inf.', _), error(syntax_error(_))).
test(l22_read_lower_inf, m1_read('1.0inf.', _), error(syntax_error(_))).
test(l22_read_short, m1_read('1.0In.', _), error(syntax_error(_))).
test(l22_read_minus_compound, (m1_read('-(1.0Inf).', T), T = -(X), float(X), X > 0), true).
test(l22_read_op_after, (m1_read('1.0Inf mod 2.', T), T = mod(X, 2), float(X)), true).
test(l22_read_back, (m1_read('f(1.0Inf, -1.0Inf, 1.5NaN).', T), m1_back(T)), true).
% 節に書いた字句も同じ値になる。mosac は値をビットのまま C++ に写す。NaN は何とも単一化しないので頭の 1.5NaN にも合わない。
l22_head(1.0Inf, plus).
l22_head(-1.0Inf, minus).
l22_head(1.5NaN, nan).
test(l22_literal_body, (X = 1.0Inf, Y = -1.0Inf, Z = 1.5NaN, X > 1.0e308, Y < -1.0e308, \+ Z =:= Z), true).
test(l22_literal_head, (X is -inf, findall(R, l22_head(X, R), L)), solutions([L = [minus]])).
test(l22_literal_head_nan, (X is nan, findall(R, l22_head(X, R), L)), solutions([L = []])).
test(l22_literal_head_scan, findall(X-R, l22_head(X, R), L), solutions([L = [1.0Inf-plus, -1.0Inf-minus, 1.5NaN-nan]])).
% レビュー L17（持ち主は A を選んだ、iso-notes 6.4.2）。6.4.2 が図形トークンにしないのは空白か % か終わりの前の . 1 字
% だけで、図形の字に続く . は同じトークンに入る。前は +. を + と終止符に読み、writeq(t('+.' - b)) の t(+. -b) が読めなかった。
test(l17_review_repro, (open_string('t(+. -b) .', S), read(S, T)), solutions([T = t('+.' - b)])).
test(l17_writeq_back, (with_output_to(atom(W), writeq(t('+.' - b))), atom_concat(W, ' .', Text), open_string(Text, S), read(S, T)), solutions([T = t('+.' - b)])).
test(l17_plus_dot_newline, (open_string('X = +.\n.', S), read(S, T), T = (_ = A)), solutions([A = '+.'])).
test(l17_plus_dot_percent, (open_string('X = +.% c\n.', S), read(S, T), T = (_ = A)), solutions([A = '+.'])).
test(l17_plus_dot_eof, (open_string('X = +.', S), read(S, _)), error(syntax_error(incomplete_term))).
test(l17_glued_next, (open_string('a = #. b.', S), read(S, _)), error(syntax_error(_))).
test(l17_space_keeps_end, (open_string('X = + . b.', S), read(S, T), read(S, U), T = (_ = A)), solutions([(A = (+), U = b)])).
test(l17_univ_kept, (open_string('X =.. Y.', S), read(S, T), T = (A =.. B), A = x, B = [x]), true).
test(l17_term_to_atom, term_to_atom(T, '+.'), solutions([T = '+.'])).
% mosatest はゴールと期待を書いて読み直す。最後が図形の字なら、足す終止符の前に空白を置く（前の X= #. は終止符が無い）。
test(l17_goal_ends_graphic, X = '#', solutions([X = '#'])).
test(l17_goal_ends_dot, X = '+.', solutions([X = '+.'])).
% レビュー M28。'.'/2 はリストの構成子（6.3.5）。前は読み手が構造体のまま作り、ゴールを IR に写すときだけ直したので、
% read/2 で読んだ '.'(a, []) は [a] と単一化せず、is_list/1 も偽だった。
test(m28_read_dot_unify, (open_string('\'.\'(a, []).', S), read(S, T), T = [a]), true).
test(m28_read_dot_list, (open_string('\'.\'(a, []).', S), read(S, T), is_list(T)), true).
test(m28_read_dot_nested, (open_string('f(\'.\'(a, \'.\'(b, []))).', S), read(S, T), T = f([A, B])), solutions([(A = a, B = b)])).
test(m28_read_dot_partial, (open_string('\'.\'(a, T).', S), read(S, L), L = [H|_]), solutions([H = a])).
test(m28_read_dot_three, (open_string('\'.\'(a, b, c).', S), read(S, T), functor(T, N, A)), solutions([(N = '.', A = 3)])).
test(m28_read_dot_infix, (op(200, xfy, '.'), open_string('x \'.\' y.', S), read(S, T), op(0, xfy, '.'), T = [H|R]), solutions([(H = x, R = y)])).
