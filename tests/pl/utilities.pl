% SPEC 第7節の段階 G の便利述語（非 ISO、レビュー M11、iso-notes の第7節 段階 G の便利述語）。
% 引数の形と誤りは SWI-Prolog に合わせる。

% tab/1,2。N は式として評価し、0 以下なら何も書かない。tab/2 は流れを先に調べる。
test(tab_spaces, (with_output_to(atom(A), (write(a), tab(3), write(b))), A == 'a   b'), true).
test(tab_expression, (with_output_to(atom(A), tab(1 + 2)), A == '   '), true).
test(tab_zero, (with_output_to(atom(A), tab(0)), A == ''), true).
test(tab_negative, (with_output_to(atom(A), tab(-2)), A == ''), true).
test(tab_var, tab(_), error(instantiation_error)).
test(tab_float, tab(1.5), error(type_error(integer, 1.5))).
test(tab_float_expression, tab(3 / 2), error(type_error(integer, 1.5))).
test(tab_not_evaluable, tab(a), error(type_error(evaluable, a/0))).
test(tab_stream, (with_output_to(atom(A), (current_output(S), tab(S, 2), write(x))), A == '  x'), true).
test(tab_stream_alias, (with_output_to(atom(A), (write(a), tab(user_output, 0), write(b))), A == ab), true).
test(tab_stream_var, tab(_, 1), error(instantiation_error)).
test(tab_stream_domain, tab(1, 1), error(domain_error(stream_or_alias, 1))).
test(tab_stream_missing, tab('$stream'(99), 1), error(existence_error(stream, '$stream'(99)))).
test(tab_stream_input, tab(user_input, 1), error(permission_error(output, stream, user_input))).
test(tab_stream_before_count, tab(user_input, a), error(permission_error(output, stream, user_input))).
test(tab_stream_count_var, tab(user_output, _), error(instantiation_error)).
