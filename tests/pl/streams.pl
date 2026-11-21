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
% 開けなかった open は、閉じた流れが持つ別名を消さない。その別名の誤りは existence_error のまま。
test(open_failed_keeps_closed_alias, (open('tests/tmp/streams_alias.txt', write, S, [alias(streams_kept)]), close(S),
  catch(open('/no/such/mosa-open-file', read, _, [alias(streams_kept)]), error(existence_error(source_sink, _), _), true),
  catch(write(streams_kept, x), error(E, _), true), E == existence_error(stream, streams_kept)), true).
test(open_write_nodir, open('/no/such/mosa-open-dir/file', write, S), error(permission_error(open, source_sink, '/no/such/mosa-open-dir/file'))).
% レビュー L5。ディレクトリは読みにも追記にも開けない（8.11.5.3 f）。前は読みが成功し、end_of_file を返した。
test(l5_open_dir_read, open('tests/pl', read, S), error(permission_error(open, source_sink, 'tests/pl'))).
test(l5_open_dir_append, open('tests/pl', append, S), error(permission_error(open, source_sink, 'tests/pl'))).
test(l5_open_dir_write, open('tests/pl', write, S), error(permission_error(open, source_sink, 'tests/pl'))).
test(l5_open_dir_options, open('tests/pl', read, S, [type(binary)]), error(permission_error(open, source_sink, 'tests/pl'))).
% レビュー 2026-11-20 の H-21。前は別名の無い流れを別名 "" で表したので、'' が別名の無い最初の流れを指し、
% 関係の無い流れを読んで閉じた。'' もほかのアトムと同じ別名で、知らなければ domain_error(stream_or_alias, '')。
% 行ごとに別名の無い流れを開いてから '' を渡し、その流れが手つかずで残ることも見る。組込のファイルごとに
% 別名を引く写しがあるので、写しごとに 1 行置く（chars、bytes、read_pred、write_pred、stream_close、stream_prop）。
% 閉じた流れも別名を持ち続け、閉じた後の '' は existence_error(stream, '') になるので、alias('') で開く行より前に置く。
h21_unknown(Goal, E, C) :-
  open_string(secret, S),
  catch(Goal, error(E, _), true),
  get_char(S, C),
  close(S).
test(h21_get_char, (h21_unknown(get_char('', _), E, C), E == domain_error(stream_or_alias, ''), C == s), true).
test(h21_get_byte, (h21_unknown(get_byte('', _), E, C), E == domain_error(stream_or_alias, ''), C == s), true).
test(h21_read, (h21_unknown(read('', _), E, C), E == domain_error(stream_or_alias, ''), C == s), true).
test(h21_write, (h21_unknown(write('', x), E, C), E == domain_error(stream_or_alias, ''), C == s), true).
test(h21_close, (h21_unknown(close(''), E, C), E == domain_error(stream_or_alias, ''), C == s), true).
test(h21_flush, (h21_unknown(flush_output(''), E, C), E == domain_error(stream_or_alias, ''), C == s), true).
test(h21_set_input, (h21_unknown(set_input(''), E, C), E == domain_error(stream_or_alias, ''), C == s), true).
% alias('') は、別名の無い流れが開いていても受け、stream_property/2 が alias('') を返し、'' で読み書きできる。
% 2 つ目の alias('') は、ほかの別名と同じく permission_error。失敗しても '' を開いたまま残さないよう後始末で閉じる。
h21_given(A, B, E) :-
  setup_call_cleanup(open_string(abc, U),
    setup_call_cleanup(open('tests/tmp/h21_alias.txt', write, T, [alias('')]),
      ( stream_property(T, alias(A)), stream_property(B, alias('')), B == T, write('', hi),
        catch(open('tests/tmp/h21_alias_2.txt', write, _, [alias('')]), error(E, _), true) ),
      close(T)),
    close(U)).
test(h21_alias_given, (h21_given(A, B, E), A == '', nonvar(B), E == permission_error(open, source_sink, alias('')),
  open('tests/tmp/h21_alias.txt', read, R), get_char(R, C1), get_char(R, C2), get_char(R, C3), close(R),
  [C1, C2, C3] == [h, i, end_of_file]), true).
% 集めた後で閉じて別名を新しい流れに譲った流れは、やり直しで alias を返さない。前は alias('') を返した。
h21_moved(Hits) :-
  open('tests/tmp/h21_moved.txt', write, S1, [alias(h21_moved)]),
  findall(A-T, ( stream_property(T, alias(A)),
                 ( A == user_input -> close(S1), h21_reopen ; true ) ),
          Hits),
  close(h21_moved).
h21_reopen :- open('tests/tmp/h21_moved.txt', write, _, [alias(h21_moved)]).
test(h21_alias_moved, (h21_moved(Hits),
  Hits == [user_input-'$stream'(0), user_output-'$stream'(1), user_error-'$stream'(2)]), true).
