test(get_byte_text, (open_string(a, S), get_byte(S, B)), error(permission_error(input, text_stream, S))).
test(put_byte_on_input, (open_string(a, S), put_byte(S, 97)), error(permission_error(output, stream, S))).
test(get_byte_user_output, get_byte(user_output, B), error(permission_error(input, stream, user_output))).
test(get_byte_current, get_byte(B), error(permission_error(input, text_stream, user_input))).
test(put_byte_user_output, put_byte(65), error(permission_error(output, text_stream, user_output))).
test(put_byte_user_input, put_byte(user_input, 65), error(permission_error(output, stream, user_input))).
test(get_byte_var, get_byte(S, B), error(instantiation_error)).
test(put_byte_var, put_byte(B), error(instantiation_error)).
test(put_byte_stream_var, put_byte(S, 1), error(instantiation_error)).
test(put_byte_open, put_byte(foo, B), error(instantiation_error)).
test(get_byte_not_stream, get_byte(foo, B), error(domain_error(stream_or_alias, foo))).
test(put_byte_not_stream, put_byte(foo, 1), error(domain_error(stream_or_alias, foo))).
test(get_byte_missing, get_byte('$stream'(99), B), error(existence_error(stream, '$stream'(99)))).
test(peek_byte_text, (open_string(ab, S), peek_byte(S, B)), error(permission_error(input, text_stream, S))).
test(peek_text_keeps, (open_string(ab, S), catch(peek_byte(S, _), _, true), get_char(S, a)), true).
test(peek_byte_output, peek_byte(user_output, B), error(permission_error(input, stream, user_output))).
test(peek_byte_current, peek_byte(B), error(permission_error(input, text_stream, user_input))).
test(peek_byte_before_type, peek_byte(a), error(permission_error(input, text_stream, user_input))).
test(peek_byte_var, peek_byte(S, B), error(instantiation_error)).
test(peek_byte_not_stream, peek_byte(foo, B), error(domain_error(stream_or_alias, foo))).
test(peek_byte_missing, peek_byte('$stream'(99), B), error(existence_error(stream, '$stream'(99)))).
test(peek_byte_closed, (open_string(a, S), close(S), peek_byte(S, B)), error(existence_error(stream, S))).
