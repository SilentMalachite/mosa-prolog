test(all_std, stream_property(S, P), solutions([
  (S = '$stream'(0), P = mode(read)),
  (S = '$stream'(0), P = alias(user_input)),
  (S = '$stream'(0), P = end_of_stream(not)),
  (S = '$stream'(0), P = eof_action(eof_code)),
  (S = '$stream'(0), P = reposition(false)),
  (S = '$stream'(0), P = type(text)),
  (S = '$stream'(0), P = encoding(utf8)),
  (S = '$stream'(1), P = mode(write)),
  (S = '$stream'(1), P = alias(user_output)),
  (S = '$stream'(1), P = end_of_stream(not)),
  (S = '$stream'(1), P = eof_action(eof_code)),
  (S = '$stream'(1), P = reposition(false)),
  (S = '$stream'(1), P = type(text)),
  (S = '$stream'(1), P = encoding(utf8)),
  (S = '$stream'(2), P = mode(write)),
  (S = '$stream'(2), P = alias(user_error)),
  (S = '$stream'(2), P = end_of_stream(not)),
  (S = '$stream'(2), P = eof_action(eof_code)),
  (S = '$stream'(2), P = reposition(false)),
  (S = '$stream'(2), P = type(text)),
  (S = '$stream'(2), P = encoding(utf8))
])).
test(input_alias, current_input(user_input), true).
test(input_term, current_input(S), solutions([S = '$stream'(0)])).
test(input_index, current_input('$stream'(0)), true).
test(input_other, current_input(user_output), fail).
test(input_bad, current_input(1), error(domain_error(stream_or_alias, 1))).
test(input_missing, current_input('$stream'(99)), error(existence_error(stream, '$stream'(99)))).
test(prop_mode, stream_property(user_input, mode(read)), true).
test(prop_mode_wrong, stream_property(user_input, mode(write)), fail).
test(prop_no_file, stream_property(user_input, file_name(_)), fail).
test(prop_no_position, stream_property(user_input, position(_)), fail).
test(prop_bad_stream, stream_property(1, mode(_)), error(domain_error(stream_or_alias, 1))).
test(prop_missing, stream_property('$stream'(99), mode(_)), error(existence_error(stream, '$stream'(99)))).
test(prop_bad, stream_property(user_input, foo), error(domain_error(stream_property, foo))).
test(prop_mode_bad, stream_property(user_input, mode(foo)), error(domain_error(stream_property, mode(foo)))).
test(prop_encoding_bad, stream_property(user_input, encoding(latin1)), error(domain_error(stream_property, encoding(latin1)))).
test(empty_at, (open_string('', S), stream_property(S, end_of_stream(at))), true).
test(empty_past, (open_string('', S), get_char(S, _), stream_property(S, end_of_stream(past))), true).
test(pos_start, (open_string(ab, S), stream_property(S, position(stream_position(0, 1, 1)))), true).
test(pos_after, (open_string(ab, S), get_char(S, _), stream_property(S, position(stream_position(1, 1, 2)))), true).
test(pos_nl, (open_string([10], S), get_char(S, _), stream_property(S, position(stream_position(1, 2, 1)))), true).
test(mem_no_alias, (open_string(a, S), stream_property(S, alias(_))), fail).
