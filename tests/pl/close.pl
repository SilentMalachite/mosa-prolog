test(flush_current, flush_output, true).
test(flush_user_output, flush_output(user_output), true).
test(flush_user_input, flush_output(user_input), error(permission_error(output, stream, user_input))).
test(flush_var, flush_output(S), error(instantiation_error)).
test(flush_not_stream, flush_output(foo), error(domain_error(stream_or_alias, foo))).
test(flush_string, (open_string(a, S), flush_output(S)), error(permission_error(output, stream, S))).
test(close_string, (open_string(ab, S), close(S), get_char(S, C)), error(existence_error(stream, S))).
test(close_twice, (open_string(a, S), close(S), close(S)), error(existence_error(stream, S))).
test(close_force, (open_string(a, S), close(S, [force(true)]), get_char(S, C)), error(existence_error(stream, S))).
test(close_bad_keeps, (open_string(a, S), catch(close(S, [foo]), _, true), get_char(S, a)), true).
test(close_var, close(S), error(instantiation_error)).
test(close_not_stream, close(foo), error(domain_error(stream_or_alias, foo))).
test(close_opt_var, close(user_input, Opts), error(instantiation_error)).
test(close_opt_type, (open_string(a, S), close(S, bar)), error(type_error(list, bar))).
test(close_opt_bad, (open_string(a, S), close(S, [force(maybe)])), error(domain_error(close_option, force(maybe)))).
test(close_missing, close('$stream'(99)), error(existence_error(stream, '$stream'(99)))).
