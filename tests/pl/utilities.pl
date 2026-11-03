% SPEC 第7節の段階 G の便利述語（非 ISO、レビュー M11、iso-notes の第7節 段階 G の便利述語）。
% 引数の形と誤りは SWI-Prolog に合わせる。

% tab/1,2。N は式として評価し、0 以下なら何も書かない。tab/2 は流れを先に調べる。
test(tab_spaces, (with_output_to(atom(A), (write(a), tab(3), write(b))), A == 'a   b'), true).
test(tab_expression, (with_output_to(atom(A), tab(1 + 2)), A == '   '), true).
test(tab_zero, (with_output_to(atom(A), tab(0)), A == ''), true).
test(tab_negative, (with_output_to(atom(A), tab(-2)), A == ''), true).
test(tab_var, tab(_), error(instantiation_error)).
test(tab_float, tab(1.5), error(type_error(integer_expression, 1.5))).
test(tab_float_expression, tab(3 / 2), error(type_error(integer_expression, 3 / 2))).
test(tab_float_whole, tab(2.0), error(type_error(integer_expression, 2.0))).
test(tab_not_evaluable, tab(a), error(type_error(evaluable, a/0))).
test(tab_stream, (with_output_to(atom(A), (current_output(S), tab(S, 2), write(x))), A == '  x'), true).
test(tab_stream_alias, (with_output_to(atom(A), (write(a), tab(user_output, 0), write(b))), A == ab), true).
test(tab_stream_var, tab(_, 1), error(instantiation_error)).
test(tab_stream_domain, tab(1, 1), error(domain_error(stream_or_alias, 1))).
test(tab_stream_missing, tab('$stream'(99), 1), error(existence_error(stream, '$stream'(99)))).
test(tab_stream_input, tab(user_input, 1), error(permission_error(output, stream, user_input))).
test(tab_stream_before_count, tab(user_input, a), error(permission_error(output, stream, user_input))).
test(tab_stream_count_var, tab(user_output, _), error(instantiation_error)).

% numbervars/3。変数を左からの前順に '$VAR'(Start)、'$VAR'(Start+1)… に束縛し、End は次の番号。
test(numbervars_basic, (T = f(X, Y, X), numbervars(T, 0, E), T == f('$VAR'(0), '$VAR'(1), '$VAR'(0)), E == 2), true).
test(numbervars_start, (numbervars(g(X, Y), 23, E), X == '$VAR'(23), Y == '$VAR'(24), E == 25), true).
test(numbervars_order, (numbervars(h(B, [A|C], B), 0, _), B == '$VAR'(0), A == '$VAR'(1), C == '$VAR'(2)), true).
test(numbervars_ground, numbervars(f(a, [b]), 3, E), solutions([E = 3])).
test(numbervars_atomic, numbervars(7, 0, E), solutions([E = 0])).
test(numbervars_var, (numbervars(X, 5, E), X == '$VAR'(5), E == 6), true).
test(numbervars_negative, (numbervars(f(X), -1, E), X == '$VAR'(-1), E == 0), true).
test(numbervars_end_bound, numbervars(f(_, _), 0, 2), true).
test(numbervars_end_differs, numbervars(f(_), 0, 5), fail).
test(numbervars_end_differs_unbound, (\+ numbervars(f(X), 0, 5), var(X)), true).
test(numbervars_end_atom, (\+ numbervars(f(X), 0, a), var(X)), true).
test(numbervars_end_shared, numbervars(f(E), 0, E), fail).
test(numbervars_existing, (T = f('$VAR'(1), '$VAR'(X)), numbervars(T, 0, E), T == f('$VAR'(1), '$VAR'('$VAR'(0))), E == 1), true).
test(numbervars_write, (T = f(X, Y, X), numbervars(T, 0, _), with_output_to(atom(A), write(T))), solutions([A = 'f(A,B,A)'])).
test(numbervars_print_26, (numbervars(X, 26, _), with_output_to(atom(A), print(X))), solutions([A = 'A1'])).
test(numbervars_cyclic, (X = f(X, Y), numbervars(X, 0, E), Y == '$VAR'(0), E == 1), true).
test(numbervars_long_list, (length(L, 200000), numbervars(L, 0, E), E == 200000, L = [F|_], F == '$VAR'(0)), true).
test(numbervars_start_var, numbervars(f(_), _, _), error(instantiation_error)).
test(numbervars_start_type, numbervars(f(_), a, _), error(type_error(integer, a))).
test(numbervars_start_float, numbervars(f(_), 1.0, _), error(type_error(integer, 1.0))).
