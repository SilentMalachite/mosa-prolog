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
% 標準ストリームの close は吐き出すだけで閉じない（iso-notes 8.11.6.1 の A、レビュー L13）。選択肢の検査は先に済ませる。
test(close_std_output_open, (close(user_output), stream_property(S, alias(user_output)), stream_property(S, mode(append))), true).
test(close_std_output_current, (close(user_output), current_output(S), stream_property(S, alias(user_output))), true).
test(close_std_input_open, (close(user_input), current_input(S), stream_property(S, alias(user_input))), true).
test(close_std_error_force, (close(user_error, [force(true)]), write(user_error, ''), stream_property(S, alias(user_error))), true).
test(close_std_by_term, (close('$stream'(1)), stream_property('$stream'(1), alias(user_output))), true).
test(close_std_twice, (close(user_output), close(user_output), close(user_input, []), close(user_input)), true).
test(close_std_bad_option, close(user_output, [foo]), error(domain_error(close_option, foo))).
test(close_std_bad_option_keeps, (catch(close(user_output, [foo]), _, true), close(user_output), stream_property(S, alias(user_output))), true).
test(close_std_option_type, close(user_error, bar), error(type_error(list, bar))).
test(close_std_keeps_other_current, (with_output_to(atom(A), (close(user_output), write(x))), A == x, current_output(S), stream_property(S, alias(user_output))), true).
test(close_std_file_still_closes, (open('tests/tmp/close_std.txt', write, S), set_output(S), close(user_output), close(S), current_output(C), stream_property(C, alias(user_output)), write(C, '')), true).
% 現在入力の流れを閉じたら user_input へ戻る（8.11.6.1、mark_closed）。
test(close_current_input_falls_back, (open_string(ab, S), set_input(S), close(S), current_input(C), stream_property(C, alias(user_input))), true).
