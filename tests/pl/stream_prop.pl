test(all_std, stream_property(S, P), solutions([
  (S = '$stream'(0), P = mode(read)),
  (S = '$stream'(0), P = input),
  (S = '$stream'(0), P = alias(user_input)),
  (S = '$stream'(0), P = end_of_stream(not)),
  (S = '$stream'(0), P = eof_action(eof_code)),
  (S = '$stream'(0), P = reposition(false)),
  (S = '$stream'(0), P = type(text)),
  (S = '$stream'(0), P = encoding(utf8)),
  (S = '$stream'(1), P = mode(write)),
  (S = '$stream'(1), P = output),
  (S = '$stream'(1), P = alias(user_output)),
  (S = '$stream'(1), P = end_of_stream(not)),
  (S = '$stream'(1), P = eof_action(eof_code)),
  (S = '$stream'(1), P = reposition(false)),
  (S = '$stream'(1), P = type(text)),
  (S = '$stream'(1), P = encoding(utf8)),
  (S = '$stream'(2), P = mode(write)),
  (S = '$stream'(2), P = output),
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
test(output_alias, current_output(user_output), true).
test(output_term, current_output(S), solutions([S = '$stream'(1)])).
test(output_index, current_output('$stream'(1)), true).
test(output_other, current_output(user_input), fail).
test(output_bad, current_output(1), error(domain_error(stream_or_alias, 1))).
test(output_missing, current_output('$stream'(99)), error(existence_error(stream, '$stream'(99)))).
test(set_same, set_input(user_input), true).
test(set_reads, (open_string(ab, S), set_input(S), get_char(C), C == a, set_input(user_input), current_input(user_input)), true).
test(set_output_stream, set_input(user_output), error(permission_error(input, stream, user_output))).
test(set_keeps, (catch(set_input(user_output), _, true), current_input(user_input)), true).
test(set_var, set_input(S), error(instantiation_error)).
test(set_bad, set_input(1), error(domain_error(stream_or_alias, 1))).
test(set_missing, set_input('$stream'(99)), error(existence_error(stream, '$stream'(99)))).
test(set_closed, (open_string(a, S), close(S), set_input(S)), error(existence_error(stream, S))).
test(out_same, set_output(user_output), true).
test(out_error, (set_output(user_error), current_output(user_error), set_output(user_output), current_output(user_output)), true).
test(out_read, (open_string(a, S), set_output(S)), error(permission_error(output, stream, S))).
test(out_keeps, (catch(set_output(user_input), _, true), current_output(user_output)), true).
test(out_var, set_output(S), error(instantiation_error)).
test(out_bad, set_output(1), error(domain_error(stream_or_alias, 1))).
test(out_missing, set_output('$stream'(99)), error(existence_error(stream, '$stream'(99)))).
test(out_closed, (open_string(a, S), close(S), set_output(S)), error(existence_error(stream, S))).
test(end_empty, (open_string('', S), at_end_of_stream(S)), true).
test(end_not, (open_string(ab, S), at_end_of_stream(S)), fail).
test(end_at, (open_string(a, S), get_char(S, a), at_end_of_stream(S)), true).
test(end_past, (open_string('', S), get_char(S, end_of_file), at_end_of_stream(S)), true).
test(end_mid, (open_string(ab, S), get_char(S, a), at_end_of_stream(S)), fail).
test(end_peek, (open_string(a, S), peek_char(S, a), \+ at_end_of_stream(S), get_char(S, a), at_end_of_stream(S)), true).
test(end_std, at_end_of_stream, fail).
test(end_output, at_end_of_stream(user_output), fail).
test(end_switch, (open_string('', S), set_input(S), at_end_of_stream, set_input(user_input), \+ at_end_of_stream), true).
test(end_var, at_end_of_stream(S), error(instantiation_error)).
test(end_bad, at_end_of_stream(1), error(domain_error(stream_or_alias, 1))).
test(end_missing, at_end_of_stream('$stream'(99)), error(existence_error(stream, '$stream'(99)))).
test(end_closed, (open_string(a, S), close(S), at_end_of_stream(S)), error(existence_error(stream, S))).
test(seek_back, (open_string(ab, S), stream_property(S, position(P)), get_char(S, _), set_stream_position(S, P), get_char(S, C), C == a), true).
test(seek_start, (open_string(ab, S), get_char(S, _), set_stream_position(S, stream_position(0, 1, 1)), get_char(S, a)), true).
test(seek_mid, (open_string(ab, S), set_stream_position(S, stream_position(1, 1, 2)), get_char(S, b)), true).
test(seek_nl, (open_string([10, 97], S), get_char(S, _), set_stream_position(S, stream_position(1, 2, 1)), get_char(S, a)), true).
test(seek_end, (open_string(a, S), get_char(S, a), get_char(S, end_of_file), set_stream_position(S, stream_position(0, 1, 1)), get_char(S, a)), true).
test(seek_at, (open_string('', S), get_char(S, end_of_file), set_stream_position(S, stream_position(0, 1, 1)), stream_property(S, end_of_stream(at))), true).
test(seek_keeps, (open_string(ab, S), get_char(S, a), catch(set_stream_position(S, foo), _, true), get_char(S, b)), true).
test(seek_var, set_stream_position(S, P), error(instantiation_error)).
test(seek_pos_var, (open_string(a, S), set_stream_position(S, P)), error(instantiation_error)).
test(seek_part, (open_string(a, S), set_stream_position(S, stream_position(0, 1, X))), error(instantiation_error)).
test(seek_bad_stream, set_stream_position(1, stream_position(0, 1, 1)), error(domain_error(stream_or_alias, 1))).
test(seek_pos_first, set_stream_position(1, P), error(instantiation_error)).
test(seek_missing, set_stream_position('$stream'(99), stream_position(0, 1, 1)), error(existence_error(stream, '$stream'(99)))).
test(seek_closed, (open_string(a, S), close(S), set_stream_position(S, stream_position(0, 1, 1))), error(existence_error(stream, S))).
test(seek_closed_foo, (open_string(a, S), close(S), set_stream_position(S, foo)), error(existence_error(stream, S))).
test(seek_std, set_stream_position(user_input, stream_position(0, 1, 1)), error(permission_error(reposition, stream, user_input))).
test(seek_foo, (open_string(a, S), set_stream_position(S, foo)), error(domain_error(stream_position, foo))).
test(seek_std_foo, set_stream_position(user_input, foo), error(domain_error(stream_position, foo))).
test(seek_neg, (open_string(a, S), set_stream_position(S, stream_position(-1, 1, 1))), error(domain_error(stream_position, stream_position(-1, 1, 1)))).
test(seek_line0, (open_string(a, S), set_stream_position(S, stream_position(0, 0, 1))), error(domain_error(stream_position, stream_position(0, 0, 1)))).
test(seek_nowhere, (open_string(a, S), set_stream_position(S, stream_position(9, 1, 1))), error(domain_error(stream_position, stream_position(9, 1, 1)))).
test(prop_mode, stream_property(user_input, mode(read)), true).
test(prop_mode_wrong, stream_property(user_input, mode(write)), fail).
test(prop_input, stream_property(user_input, input), true).
test(prop_input_not_output, stream_property(user_input, output), fail).
test(prop_output, stream_property(user_output, output), true).
test(prop_output_not_input, stream_property(user_output, input), fail).
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
