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
