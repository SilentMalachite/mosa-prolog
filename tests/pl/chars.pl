test(peek_then_get, (open_string('あb', S), peek_char(S, C), get_char(S, D), C == D, C == 'あ'), true).
test(peek_keeps, (open_string(ab, S), peek_char(S, A), peek_char(S, B), A == a, B == a), true).
test(get_two, (open_string(ab, S), get_char(S, A), get_char(S, B), A == a, B == b), true).
test(get_unify, (open_string(ab, S), get_char(S, a)), true).
test(get_unify_fail, (open_string(ab, S), get_char(S, b)), fail).
test(get_fail_consumes, (open_string(ab, S), (get_char(S, b) ; true), get_char(S, C), C == b), true).
test(peek_fail_keeps, (open_string(ab, S), (peek_char(S, b) ; true), get_char(S, C), C == a), true).
test(eof_empty, (open_string('', S), get_char(S, A), get_char(S, B), A == end_of_file, B == end_of_file), true).
test(eof_after_char, (open_string(a, S), get_char(S, A), get_char(S, B), get_char(S, C), A == a, B == end_of_file, C == end_of_file), true).
test(peek_eof_stays, (open_string('', S), peek_char(S, A), get_char(S, B), A == end_of_file, B == end_of_file), true).
test(get_nl, (open_string([10], S), get_char(S, '\n')), true).
test(get_char_type, get_char(1), error(type_error(in_character, 1))).
test(get_stream_var, get_char(S, C), error(instantiation_error)).
test(get_not_stream, get_char(foo, C), error(domain_error(stream_or_alias, foo))).
test(get_missing, get_char('$stream'(99), C), error(existence_error(stream, '$stream'(99)))).
test(get_user_output, get_char(user_output, C), error(permission_error(input, stream, user_output))).
test(peek_user_output, peek_char(user_output, C), error(permission_error(input, stream, user_output))).
test(put_char_var, put_char(C), error(instantiation_error)).
test(put_stream_var, put_char(S, a), error(instantiation_error)).
test(put_char_open, put_char(foo, C), error(instantiation_error)).
test(put_char_type, put_char(1), error(type_error(character, 1))).
test(put_end_of_file, put_char(end_of_file), error(type_error(character, end_of_file))).
test(put_not_stream, put_char(foo, a), error(domain_error(stream_or_alias, foo))).
test(put_user_input, put_char(user_input, a), error(permission_error(output, stream, user_input))).
test(put_on_input, (open_string(a, S), put_char(S, b)), error(permission_error(output, stream, S))).
test(get_on_input_ok, (open_string(a, S), get_char(S, a)), true).
test(code_two, (open_string(ab, S), get_code(S, A), get_code(S, B), A == 97, B == 98), true).
test(code_unify, (open_string(ab, S), get_code(S, 97)), true).
test(code_unify_fail, (open_string(ab, S), get_code(S, 98)), fail).
test(code_fail_consumes, (open_string(ab, S), (get_code(S, 98) ; true), get_code(S, C), C == 98), true).
test(peek_code_keeps, (open_string(ab, S), peek_code(S, A), peek_code(S, B), A == 97, B == 97), true).
test(peek_code_fail_keeps, (open_string(ab, S), (peek_code(S, 98) ; true), get_code(S, C), C == 97), true).
test(code_eof, (open_string('', S), get_code(S, A), get_code(S, B), A == -1, B == -1), true).
test(peek_code_at, (open_string('', S), peek_code(S, -1), stream_property(S, end_of_stream(at))), true).
test(get_code_past, (open_string('', S), get_code(S, -1), stream_property(S, end_of_stream(past))), true).
test(code_nl, (open_string([10], S), get_code(S, 10)), true).
test(code_current, (open_string(ab, S), set_input(S), get_code(C), set_input(user_input), C == 97), true).
test(peek_current, (open_string(ab, S), set_input(S), peek_code(C), get_code(D), set_input(user_input), C == 97, D == 97), true).
test(code_type, get_code(a), error(type_error(integer, a))).
test(code_type_stream, (open_string(a, S), get_code(S, a)), error(type_error(integer, a))).
test(peek_code_type, peek_code(a), error(type_error(integer, a))).
test(code_range, (open_string(ab, S), get_code(S, -2)), error(representation_error(in_character_code))).
test(code_range_keeps, (open_string(ab, S), catch(get_code(S, -2), _, true), get_code(S, 97)), true).
test(peek_range_keeps, (open_string(ab, S), catch(peek_code(S, -2), _, true), get_code(S, 97)), true).
test(code_surrogate, (open_string(a, S), peek_code(S, 55296)), error(representation_error(in_character_code))).
test(code_big, (open_string(a, S), get_code(S, 1114112)), error(representation_error(in_character_code))).
test(code_var, get_code(S, C), error(instantiation_error)).
test(code_alias, get_code(foo, C), error(domain_error(stream_or_alias, foo))).
test(code_before_type, get_code(foo, a), error(domain_error(stream_or_alias, foo))).
test(code_missing, get_code('$stream'(99), C), error(existence_error(stream, '$stream'(99)))).
test(code_output, get_code(user_output, C), error(permission_error(input, stream, user_output))).
test(peek_code_output, peek_code(user_output, C), error(permission_error(input, stream, user_output))).
test(put_code_var, put_code(C), error(instantiation_error)).
test(put_code_stream_var, put_code(S, 97), error(instantiation_error)).
test(put_code_open, put_code(foo, C), error(instantiation_error)).
test(put_code_type, put_code(a), error(type_error(integer, a))).
test(put_code_end, put_code(-1), error(representation_error(character_code))).
test(put_code_big, put_code(1114112), error(representation_error(character_code))).
test(put_code_surrogate, put_code(55296), error(representation_error(character_code))).
test(put_code_not_stream, put_code(foo, 97), error(domain_error(stream_or_alias, foo))).
test(put_code_before_type, put_code(foo, a), error(domain_error(stream_or_alias, foo))).
test(put_code_user_input, put_code(user_input, 97), error(permission_error(output, stream, user_input))).
test(put_code_input_type, put_code(user_input, a), error(permission_error(output, stream, user_input))).
test(put_code_on_input, (open_string(a, S), put_code(S, 98)), error(permission_error(output, stream, S))).
test(put_code_closed, (open_string(a, S), close(S), put_code(S, 97)), error(existence_error(stream, S))).
test(nl_var, nl(S), error(instantiation_error)).
test(nl_bad, nl(1), error(domain_error(stream_or_alias, 1))).
test(nl_missing, nl('$stream'(99)), error(existence_error(stream, '$stream'(99)))).
test(nl_input, nl(user_input), error(permission_error(output, stream, user_input))).
test(nl_index, nl('$stream'(0)), error(permission_error(output, stream, '$stream'(0)))).
test(nl_on_input, (open_string(a, S), nl(S)), error(permission_error(output, stream, S))).
test(nl_closed, (open_string(a, S), close(S), nl(S)), error(existence_error(stream, S))).
test(invalid_utf8_replace, (set_prolog_flag(invalid_utf8, replace), current_prolog_flag(invalid_utf8, replace)), true).
test(invalid_utf8_error, (set_prolog_flag(invalid_utf8, error), current_prolog_flag(invalid_utf8, error)), true).

% レビュー L6。壊れた UTF-8 の並びは、正しい並びの頭として通る最長の部分（Unicode 15.0 の 3.9 節の maximal subpart）を
% 1 文字として進む。replace はそれを U+FFFD 1 つにし、error は進めてから representation_error(character) を出す
% （iso-notes 第4節の決定 A）。前は先頭の 1 バイトだけを 1 文字とし、error では進めなかった。
l6_file(Name, Bytes, Path) :-
    atom_concat('tests/tmp/l6_', Name, Path),
    open(Path, write, S, [type(binary)]), l6_put(Bytes, S), close(S).
l6_put([], _).
l6_put([B|Bs], S) :- put_byte(S, B), l6_put(Bs, S).
l6_codes(S, Cs) :- get_code(S, C), ( C =:= -1 -> Cs = [] ; Cs = [C|Rest], l6_codes(S, Rest) ).
% 旗は誤りでも失敗でも error に戻す。replace のまま残ると、後のテストが紛らわしく落ちる。
l6_with_replace(Goal) :-
    setup_call_cleanup(set_prolog_flag(invalid_utf8, replace), Goal, set_prolog_flag(invalid_utf8, error)).
l6_replaced(Name, Bytes, Cs) :-
    l6_file(Name, Bytes, Path),
    l6_with_replace((open(Path, read, S), l6_codes(S, Cs), close(S))).
% Bytes を error で read し続け、誤りの種類と読めた項を並べる。
l6_read_all(Name, Bytes, Ts) :-
    l6_file(Name, Bytes, Path), open(Path, read, S), l6_terms(S, Ts0), close(S), l6_kinds(Ts0, Ts).
% error で 1 つずつ読み、誤りは err に置き換える。進まない実装でも止まるよう、8 回で打ち切る。
l6_chars(S, Cs) :- l6_chars(S, 8, Cs).
l6_chars(_, 0, [cut]) :- !.
l6_chars(S, N, Cs) :-
    catch(get_char(S, C0), error(representation_error(character), _), C0 = err),
    M is N - 1,
    ( C0 == end_of_file -> Cs = [] ; Cs = [C0|Rest], l6_chars(S, M, Rest) ).
l6_terms(S, Ts) :- l6_terms(S, 8, Ts).
l6_terms(_, 0, [cut]) :- !.
l6_terms(S, N, Ts) :-
    catch(read(S, T0), error(E, _), T0 = err(E)),
    M is N - 1,
    ( T0 == end_of_file -> Ts = [] ; Ts = [T0|Rest], l6_terms(S, M, Rest) ).
l6_err_kind(err(representation_error(character)), rep).
l6_err_kind(err(syntax_error(_)), syntax).
l6_err_kind(T, T) :- T \= err(_).
l6_kinds([], []).
l6_kinds([T|Ts], [K|Ks]) :- once(l6_err_kind(T, K)), l6_kinds(Ts, Ks).

test(l6_replace_truncated, l6_replaced(truncated, [0'a, 0xE3, 0x81, 0'b, 0xE3, 0x81], Cs), solutions([Cs = [97, 65533, 98, 65533]])).
test(l6_replace_cut_by_ascii, l6_replaced(cut, [0xE3, 0x81, 0'A], Cs), solutions([Cs = [65533, 65]])).
test(l6_replace_four_cut, l6_replaced(four, [0xF0, 0x9F, 0x98, 0'A], Cs), solutions([Cs = [65533, 65]])).
test(l6_replace_overlong, l6_replaced(overlong, [0xE0, 0x80, 0'A], Cs), solutions([Cs = [65533, 65533, 65]])).
test(l6_replace_surrogate, l6_replaced(surrogate, [0xED, 0xA0, 0x80], Cs), solutions([Cs = [65533, 65533, 65533]])).
test(l6_replace_bad_lead, l6_replaced(lead, [0xFF, 0xC0, 0x80, 0'A], Cs), solutions([Cs = [65533, 65533, 65533, 65]])).
% 表 3-7 の残りの行。F4 の次は 80..8F、F0 の次は 90..BF だけが続く。E1 80 は C0 の前で切れた 1 つ。F5 は先頭になれない。
test(l6_replace_table, l6_replaced(table, [0xF4, 0x90, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80, 0xE1, 0x80, 0xC0, 0xF5, 0'A], Cs), solutions([Cs = [65533, 65533, 65533, 65533, 65533, 65533, 65533, 65533, 65533, 65533, 65533, 65]])).
test(l6_error_advances, (l6_file(error, [0'a, 0xE3, 0x81, 0'b], Path), open(Path, read, S), l6_chars(S, Cs), close(S)), solutions([Cs = [a, err, b]])).
test(l6_error_position, (l6_file(position, [0'a, 0xE3, 0x81, 0'b], Path), open(Path, read, S), get_char(S, _), catch(get_char(S, _), _, true), stream_property(S, position(stream_position(N, _, _))), get_char(S, C), close(S)), solutions([(N = 2, C = b)])).
test(l6_peek_keeps, (l6_file(peek, [0xFF, 0'a], Path), open(Path, read, S), catch(peek_char(S, _), _, true), catch(get_char(S, _), error(E, _), true), get_char(S, C), close(S)), solutions([(E = representation_error(character), C = a)])).
% read/1 は、replace で読んだのと同じ所（その項の終止符の後）まで進めてから誤りを出し、その項を捨てる。
test(l6_read_error_advances, l6_read_all(read, [0'f, 0'o, 0'o, 0'(, 0xE3, 0'), 0'., 0'\n, 0'b, 0'a, 0'r, 0'., 0'\n], Ts), solutions([Ts = [rep, bar]])).
test(l6_read_quoted, l6_read_all(quoted, [0'x, 0'(, 0'', 0'c, 0'a, 0'f, 0xE9, 0'', 0'), 0'., 0'\n, 0'y, 0'., 0'\n], Ts), solutions([Ts = [rep, y]])).
test(l6_read_block_comment, l6_read_all(block, [0'/, 0'*, 0' , 0xE9, 0' , 0'*, 0'/, 0' , 0'o, 0'k, 0'., 0'\n, 0'n, 0'., 0'\n], Ts), solutions([Ts = [rep, n]])).
% 注釈の中の壊れた並びは、その後ろの項と一緒に読まれる。replace はその項を返すので、error はその項まで捨てる。
test(l6_read_line_comment, l6_read_all(line, [0'%, 0' , 0xE9, 0' , 0'x, 0'., 0'\n, 0'r, 0'., 0'\n, 0'o, 0'k, 0'., 0'\n], Ts), solutions([Ts = [rep, ok]])).
% 置き換えた U+FFFD は図形の字なので、続けた . は同じトークンに入る（6.4.2、レビュー L17）。空白で終止符を分ける。
test(l6_read_bom_error, l6_read_all(bom, [0xEF, 0xBB, 0xBF, 0xE9, 0' , 0'., 0'\n, 0'o, 0'k, 0'., 0'\n], Ts), solutions([Ts = [rep, ok]])).
% 構文の誤りの読み飛ばしの中の壊れた並びも置き換えてから飛ばす。前は注釈の中の壊れた並びで注釈が切れ、注釈の中の
% 終止符で止まって、次の read が注釈の中の delete_all を返した（L6 の Codex レビュー）。
test(l6_read_skip_comment, l6_read_all(skip, [0'f, 0'(, 0xFF, 0' , 0'a, 0' , 0'/, 0'*, 0' , 0xFF, 0'., 0' , 0'd, 0'., 0' , 0'*, 0'/, 0'), 0'., 0' , 0'b, 0'., 0'\n], Ts), solutions([Ts = [rep, b]])).
test(l6_read_skip_comment_replace, (l6_file(skip_replace, [0'f, 0'(, 0xFF, 0' , 0'a, 0' , 0'/, 0'*, 0' , 0xFF, 0'., 0' , 0'd, 0'., 0' , 0'*, 0'/, 0'), 0'., 0' , 0'b, 0'., 0'\n], Path), l6_with_replace((open(Path, read, S), l6_terms(S, Ts0), close(S))), l6_kinds(Ts0, Ts)), solutions([Ts = [syntax, b]])).
% char_conversion は行と列を変える。置き換える位置はバイト位置で決める。前は改行を空白に変換すると引用符を置き換え、
% 読み直しが止まらなかった（同）。
l6_newline_as_space(Goal) :-
    setup_call_cleanup((set_prolog_flag(char_conversion, on), char_conversion('\n', ' ')), Goal,
                       (char_conversion('\n', '\n'), set_prolog_flag(char_conversion, off))).
test(l6_read_char_conversion, l6_newline_as_space(l6_read_all(conv, [0'\n, 0'', 0xE9, 0'', 0'., 0'\n], Ts)), solutions([Ts = [rep]])).
% F8..FF は先頭になれない。前は F8 90 80 80 を正しい字と取り、replace の read が同じ誤りを出し続けた（L6 の Codex レビュー 2 回目）。
test(l6_read_f8_replace, (l6_file(f8, [0'', 0xF8, 0x90, 0x80, 0x80, 0'', 0'., 0' , 0'o, 0'k, 0'., 0'\n], Path), l6_with_replace((open(Path, read, S), read(S, A), read(S, B), close(S))), atom_codes(A, Cs)), solutions([(Cs = [65533, 65533, 65533, 65533], B = ok)])).
test(l6_read_f8_error, l6_read_all(f8_error, [0'', 0xF8, 0x90, 0x80, 0x80, 0'', 0'., 0' , 0'o, 0'k, 0'., 0'\n], Ts), solutions([Ts = [rep, ok]])).
% 先頭の BOM は字句解析が列に数えない。前は置き換える位置が 1 字ずれ、replace の read が止まらなかった（L6 の敵対的レビュー A4）。
test(l6_read_bom_replace, (l6_file(bom_replace, [0xEF, 0xBB, 0xBF, 0'', 0xE9, 0'', 0'., 0'\n], Path), l6_with_replace((open(Path, read, S), read(S, T), close(S))), atom_codes(T, Cs)), solutions([Cs = [65533]])).
% U+FFFD は図形の字なので、続けた . は同じトークンに入る（6.4.2、レビュー L17）。空白で終止符を分ける。
test(l6_read_bom_keeps_next, (l6_file(bom_next, [0xEF, 0xBB, 0xBF, 0'a, 0'b, 0xE9, 0' , 0'., 0'\n], Path), l6_with_replace((open(Path, read, S), catch(read(S, _), error(syntax_error(_), context(_, syntax(_, Token))), true), close(S))), atom_codes(Token, Cs)), solutions([Cs = [65533]])).
% 空白を置かない元の入力（レビュー L17 の敵対的レビュー A1）。置き換えた U+FFFD が . を取り込むので、構文エラーの
% 読み飛ばしは次の終止符まで進み、後ろの ok. も一緒に消える。replace の誤りの字句も U+FFFD と . の 2 字になる。
test(l17_bad_byte_takes_dot, l6_read_all(bom_glued, [0xEF, 0xBB, 0xBF, 0xE9, 0'., 0'\n, 0'o, 0'k, 0'., 0'\n], Ts), solutions([Ts = [rep]])).
test(l17_bad_byte_glued_token, (l6_file(bom_glued_next, [0xEF, 0xBB, 0xBF, 0'a, 0'b, 0xE9, 0'., 0'\n], Path), l6_with_replace((open(Path, read, S), catch(read(S, _), error(syntax_error(_), context(_, syntax(_, Token))), true), close(S))), atom_codes(Token, Cs)), solutions([Cs = [65533, 46]])).
% stream_property の位置へ戻すと、同じ字から読み直す。前は位置の数え方が get_char とずれ、違うバイトに立った（同 A2、A3）。
test(l6_reposition_error, (l6_file(repos, [0'a, 0xE3, 0x81, 0'b], Path), open(Path, read, S, [reposition(true)]), get_char(S, _), catch(get_char(S, _), _, true), stream_property(S, position(P)), get_char(S, B), set_stream_position(S, P), get_char(S, C), close(S)), solutions([(B = b, C = b)])).
test(l6_reposition_surrogate, (l6_file(repos_sur, [0xED, 0xA0, 0x80, 0'b], Path), l6_with_replace((open(Path, read, S, [reposition(true)]), get_code(S, _), stream_property(S, position(P)), get_code(S, _), get_code(S, _), set_stream_position(S, P), l6_codes(S, Cs), close(S)))), solutions([Cs = [65533, 65533, 98]])).
test(l6_reposition_back_over_error, (l6_file(repos_back, [0'a, 0xE3, 0x81, 0'b], Path), open(Path, read, S, [reposition(true)]), get_char(S, _), stream_property(S, position(P)), catch(get_char(S, _), _, true), set_stream_position(S, P), catch(get_char(S, _), error(E, _), true), close(S)), solutions([E = representation_error(character)])).
test(l6_append_position, (l6_file(append, [0'a, 0xED, 0xA0, 0x80, 0'b], Path), open(Path, append, S), stream_property(S, position(stream_position(N, _, _))), close(S)), solutions([N = 5])).
test(l6_read_replace_one, (l6_file(read_replace, [0'f, 0'(, 0xE3, 0x81, 0'), 0'., 0'\n], Path), l6_with_replace((open(Path, read, S), read(S, T), close(S))), T = f(A), atom_codes(A, Cs)), solutions([Cs = [65533]])).
