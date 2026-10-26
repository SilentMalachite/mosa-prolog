test(open_string_atom, (open_string(ab, S), functor(S, '$stream', 1)), true).
test(open_string_codes, (open_string([97, 98], S), functor(S, '$stream', 1)), true).
test(open_string_empty_atom, (open_string('', S), functor(S, '$stream', 1)), true).
test(open_string_empty_list, (open_string([], S), functor(S, '$stream', 1)), true).
test(open_string_var, open_string(X, S), error(instantiation_error)).
test(open_string_partial, open_string([97|X], S), error(instantiation_error)).
test(open_string_chars, open_string([a], S), error(type_error(integer, a))).
test(open_string_int, open_string(1, S), error(type_error(atom, 1))).
test(open_string_bad_list, open_string([97|98], S), error(type_error(list, [97|98]))).
test(open_string_bound, open_string(ab, s), error(uninstantiation_error(s))).
test(open_string_big, open_string([1114112], S), error(representation_error(character_code))).
test(open_string_neg, open_string([-1], S), error(representation_error(character_code))).
test(open_source_var, open(F, read, S), error(instantiation_error)).
test(open_mode_var, open(a, M, S), error(instantiation_error)).
test(open_opt_var, open(a, read, S, Opts), error(instantiation_error)).
test(open_opt_elem_var, open(a, read, S, [foo, X]), error(instantiation_error)).
test(open_opt_partial, open(a, read, S, [type(text)|X]), error(instantiation_error)).
test(open_opt_var_before_mode, open(a, 1, S, X), error(instantiation_error)).
test(open_mode_type, open(a, 1, S), error(type_error(atom, 1))).
test(open_mode_before_list, open(a, 1, S, foo), error(type_error(atom, 1))).
test(open_list, open(a, read, S, foo), error(type_error(list, foo))).
test(open_stream_before_source, open(1, read, s), error(uninstantiation_error(s))).
test(open_source_int, open(1, read, S), error(domain_error(source_sink, 1))).
test(open_mode_bad, open(a, foo, S), error(domain_error(io_mode, foo))).
test(open_mode_before_option, open(a, foo, S, [eof_action(drop)]), error(domain_error(io_mode, foo))).
test(open_opt_atom, open(a, read, S, [foo]), error(domain_error(stream_option, foo))).
test(open_alias_int, open(a, read, S, [alias(1)]), error(domain_error(stream_option, alias(1)))).
test(open_eof_int, open(a, read, S, [eof_action(1)]), error(domain_error(stream_option, eof_action(1)))).
test(open_reposition_int, open(a, read, S, [reposition(1)]), error(domain_error(stream_option, reposition(1)))).
test(open_type_int, open(a, read, S, [type(1)]), error(domain_error(stream_option, type(1)))).
test(open_type_bad, open(a, read, S, [type(foo)]), error(domain_error(stream_option, type(foo)))).
test(open_encoding_bad, open(a, read, S, [encoding(latin1)]), error(domain_error(stream_option, encoding(latin1)))).
test(open_eof_bad, open(a, read, S, [eof_action(drop)]), error(domain_error(stream_option, eof_action(drop)))).
test(open_two_alias, open(a, read, S, [alias(aa), alias(bb)]), error(domain_error(stream_option, alias(bb)))).
test(open_missing, open('/no/such/mosa-open-file', read, S), error(existence_error(source_sink, '/no/such/mosa-open-file'))).
test(open_utf8_missing, open('/no/such/mosa-open-file', read, S, [encoding(utf8)]), error(existence_error(source_sink, '/no/such/mosa-open-file'))).
test(open_alias_after_missing, open('/no/such/mosa-open-file', read, S, [alias(user_input)]), error(existence_error(source_sink, '/no/such/mosa-open-file'))).
test(open_alias_user_input, open('/no/such/mosa-open-file', write, S, [alias(user_input)]), error(permission_error(open, source_sink, alias(user_input)))).
test(open_write_nodir, open('/no/such/mosa-open-dir/file', write, S), error(permission_error(open, source_sink, '/no/such/mosa-open-dir/file'))).
