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
