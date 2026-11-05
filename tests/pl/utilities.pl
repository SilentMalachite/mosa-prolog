% SPEC 第7節の段階 G の便利述語（非 ISO、レビュー M11、iso-notes の第7節 段階 G の便利述語）。
% 引数の形と誤りは SWI-Prolog に合わせる。
:- use_module(library(lists)).

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

% sort/4。Key が 0 なら項全体、正の整数ならその引数で比べる。@< と @> は鍵の等しい要素の最初だけを残し、
% @=< と @>= は安定。検査は Key、Order、List、要素の鍵の順で、Sorted は単一化するだけ（SWI-Prolog の pl-list.c）。
test(sort4_key_asc, sort(1, @<, [f(2, a), f(1, z), f(1, b), f(3, c)], L), solutions([L = [f(1, z), f(2, a), f(3, c)]])).
test(sort4_key_asc_dups, sort(1, @=<, [f(2, a), f(1, z), f(1, b), f(3, c)], L), solutions([L = [f(1, z), f(1, b), f(2, a), f(3, c)]])).
test(sort4_key_desc, sort(1, @>, [f(2, a), f(1, z), f(1, b), f(3, c)], L), solutions([L = [f(3, c), f(2, a), f(1, z)]])).
test(sort4_key_desc_dups, sort(1, @>=, [f(2, a), f(1, z), f(1, b), f(3, c)], L), solutions([L = [f(3, c), f(2, a), f(1, z), f(1, b)]])).
test(sort4_whole_asc, sort(0, @<, [c, a, b, a], L), solutions([L = [a, b, c]])).
test(sort4_whole_desc_dups, sort(0, @>=, [1, 2, 3, 2], L), solutions([L = [3, 2, 2, 1]])).
test(sort4_second_arg, sort(2, @<, [p(x, 3), p(y, 1), p(z, 2)], L), solutions([L = [p(y, 1), p(z, 2), p(x, 3)]])).
test(sort4_list_elements, sort(1, @<, [[b|x], [a|y]], L), solutions([L = [[a|y], [b|x]]])).
test(sort4_synonyms, (sort(0, <, [b, a, b], A), sort(0, =<, [b, a, b], B), sort(0, >, [a, b, a], C), sort(0, >=, [a, b, a], D)), solutions([(A = [a, b], B = [a, b, b], C = [b, a], D = [b, a, a])])).
test(sort4_empty, sort(3, @<, [], L), solutions([L = []])).
test(sort4_sorted_bound, sort(0, @<, [b, a], [a, b]), true).
test(sort4_sorted_not_list, sort(0, @<, [b, a], foo), fail).
test(sort4_sorted_partial, sort(0, @<, [c, a, b], [X|_]), solutions([X = a])).
test(sort4_key_var, sort(_, @<, [], _), error(instantiation_error)).
test(sort4_key_negative, sort(-1, @<, [], _), error(domain_error(not_less_than_one, -1))).
test(sort4_key_float, sort(1.5, @<, [], _), error(type_error(sort_key, 1.5))).
test(sort4_key_atom, sort(a, @<, [], _), error(type_error(sort_key, a))).
test(sort4_key_before_order, sort(a, foo, [], _), error(type_error(sort_key, a))).
test(sort4_order_var, sort(0, _, [], _), error(instantiation_error)).
test(sort4_order_type, sort(0, 1, [], _), error(type_error(atom, 1))).
test(sort4_order_domain, sort(0, foo, [], _), error(domain_error(order, foo))).
test(sort4_order_before_list, sort(0, foo, bar, _), error(domain_error(order, foo))).
test(sort4_list_var, sort(0, @<, _, _), error(instantiation_error)).
test(sort4_list_partial, sort(0, @<, [a|_], _), error(instantiation_error)).
test(sort4_list_type, sort(0, @<, foo, _), error(type_error(list, foo))).
test(sort4_list_tail, sort(0, @<, [a|b], _), error(type_error(list, [a|b]))).
test(sort4_element_var, sort(1, @<, [f(1), _], _), error(instantiation_error)).
test(sort4_element_atomic, sort(1, @<, [f(1), a], _), error(type_error(compound, a))).
test(sort4_element_arity, sort(2, @<, [f(1)], _), error(existence_error(argument, 2, f(1)))).
test(sort4_element_order, sort(2, @<, [a, f(1)], _), error(type_error(compound, a))).
test(sort4_whole_vars, (sort(0, @<, [X, Y, X], L), L == [X, Y] ; sort(0, @<, [X, Y, X], L), L == [Y, X]), true).

% term_to_atom/2。Atom が変数なら Term を引用して書き（numbervars は使わない）、そうでなければ Atom の字面を読んで
% Term と単一化する。終止符は無くてよく、最初の項の後ろは読まない（SWI-Prolog の pl-read.c）。
test(term_to_atom_write, term_to_atom(f(a, 'B c', [1, 2]), A), solutions([A = 'f(a,\'B c\',[1,2])'])).
test(term_to_atom_write_ops, term_to_atom((a :- b, c), A), solutions([A = 'a:-b,c'])).
test(term_to_atom_write_var, (term_to_atom(f(X, Y, X), A), term_to_atom(T, A), T = f(P, Q, R), P == R, P \== Q, var(P), var(Q)), true).
test(term_to_atom_write_no_numbervars, term_to_atom('$VAR'(1), A), solutions([A = '\'$VAR\'(1)'])).
test(term_to_atom_both_var, (term_to_atom(T, A), atom(A), var(T)), true).
test(term_to_atom_read, term_to_atom(T, 'foo(X, Y, X)'), true).
test(term_to_atom_read_shape, (term_to_atom(T, 'foo(X, Y, X)'), T = foo(P, Q, R), P == R, P \== Q), true).
test(term_to_atom_read_ops, term_to_atom(T, 'a :- b, c'), solutions([T = (a :- b, c)])).
test(term_to_atom_read_end, term_to_atom(T, 'foo.'), solutions([T = foo])).
test(term_to_atom_read_rest, term_to_atom(T, 'foo. bar'), solutions([T = foo])).
test(term_to_atom_read_comment, term_to_atom(T, 'foo % c'), solutions([T = foo])).
test(term_to_atom_both_bound, term_to_atom(f(X), 'f(1)'), solutions([X = 1])).
test(term_to_atom_both_bound_differ, term_to_atom(g(_), 'f(1)'), fail).
test(term_to_atom_number, term_to_atom(T, 12), solutions([T = 12])).
test(term_to_atom_float, term_to_atom(T, -1.5), solutions([T = -1.5])).
test(term_to_atom_codes, term_to_atom(T, [0'f, 0'(, 0'x, 0')]), solutions([T = f(x)])).
test(term_to_atom_chars, term_to_atom(T, [g, '(', a, ')']), solutions([T = g(a)])).
test(term_to_atom_nil, term_to_atom(T, []), solutions([T = []])).
test(term_to_atom_round_trip, (term_to_atom(f('a b', "c", -(1), - 1, 1 - -1, [x|Y]), A), term_to_atom(T, A), T = f('a b', _, -(1), _, _, [x|Z]), var(Z), Y \== Z), true).
test(term_to_atom_empty, term_to_atom(_, ''), error(syntax_error(incomplete_term))).
test(term_to_atom_blank, term_to_atom(_, '  '), error(syntax_error(incomplete_term))).
test(term_to_atom_open, term_to_atom(_, 'f('), error(syntax_error(incomplete_term))).
test(term_to_atom_bad, term_to_atom(_, 'f(a b)'), error(syntax_error(_))).
test(term_to_atom_partial, term_to_atom(_, [0'a|_]), error(instantiation_error)).
test(term_to_atom_element_var, term_to_atom(_, [0'a, _]), error(instantiation_error)).
test(term_to_atom_compound, term_to_atom(_, f(x)), error(type_error(string, f(x)))).
test(term_to_atom_mixed, term_to_atom(_, [a, 0'b]), error(type_error(string, [a, 0'b]))).
test(term_to_atom_bad_tail, term_to_atom(_, [0'a|b]), error(type_error(string, [0'a|b]))).

% nb_setval/2、nb_getval/2。値は置いたときに写し、後戻りでも消えない。SWI-Prolog の nb_getval は同じ項を返すが、
% Mosa のヒープは後戻りで巻き戻るので、読むたびに写しを返す（iso-notes 第7節 段階 G の便利述語）。
test(nb_set_get, (nb_setval(m11_k1, f(a, [1, 2])), nb_getval(m11_k1, V)), solutions([V = f(a, [1, 2])])).
test(nb_overwrite, (nb_setval(m11_k2, one), nb_setval(m11_k2, two), nb_getval(m11_k2, V)), solutions([V = two])).
test(nb_survives_backtracking, ((nb_setval(m11_k3, before), (nb_setval(m11_k3, after), fail ; true)), nb_getval(m11_k3, V)), solutions([V = after])).
test(nb_copied_at_set, (X = g(Y), nb_setval(m11_k4, X), Y = bound, nb_getval(m11_k4, V), V = g(Z), var(Z)), true).
test(nb_get_fresh, (nb_setval(m11_k5, h(_)), nb_getval(m11_k5, A), nb_getval(m11_k5, B), A = h(P), B = h(Q), P \== Q), true).
test(nb_get_bound_value, (nb_setval(m11_k6, 3), nb_getval(m11_k6, 3)), true).
test(nb_get_differs, (nb_setval(m11_k7, 3), nb_getval(m11_k7, 4)), fail).
test(nb_cyclic, (X = f(X, a), nb_setval(m11_k8, X), nb_getval(m11_k8, V), V = f(W, A), A == a, W = f(_, A2), A2 == a), true).
test(nb_long_list, (numlist_m11(100000, L), nb_setval(m11_k9, L), nb_getval(m11_k9, V), length(V, N)), solutions([N = 100000])).
test(nb_after_findall, (findall(x, nb_setval(m11_k10, inside), _), nb_getval(m11_k10, V)), solutions([V = inside])).
test(nb_set_key_var, nb_setval(_, a), error(instantiation_error)).
test(nb_set_key_type, nb_setval(f(x), a), error(type_error(atom, f(x)))).
test(nb_set_key_number, nb_setval(1, a), error(type_error(atom, 1))).
test(nb_get_key_var, nb_getval(_, _), error(instantiation_error)).
test(nb_get_key_type, nb_getval(1, _), error(type_error(atom, 1))).
test(nb_get_missing, nb_getval(m11_never_set, _), error(existence_error(variable, m11_never_set))).

numlist_m11(N, L) :- numlist_m11(1, N, L).
numlist_m11(I, N, []) :- I > N, !.
numlist_m11(I, N, [I|T]) :- J is I + 1, numlist_m11(J, N, T).

% setup_call_cleanup/3（ADR 0016）。Setup は once/1 として動き、Cleanup は Goal が選択点を残さずに抜けたとき、
% 失敗したとき、例外を投げたとき、選択点を切られたときに 1 回だけ動く。
test(scc_det_exit, (with_output_to(atom(A), (setup_call_cleanup(write(s), write(g), write(c)), write(after))), A == sgcafter), true).
test(scc_det_bindings, setup_call_cleanup(true, X = 1, Y = X), solutions([(X = 1, Y = 1)])).
test(scc_fail, (with_output_to(atom(A), (setup_call_cleanup(true, fail, write(c)) ; write(alt))), A == calt), true).
test(scc_fail_bindings, (((setup_call_cleanup(X = s, (Y = g, fail), nb_setval(m11_scc_f, X-Y))) ; true), nb_getval(m11_scc_f, P-Q), P == s, var(Q)), true).
test(scc_exhausted, (with_output_to(atom(A), forall(setup_call_cleanup(true, member(X, [1, 2]), write(c)), write(X))), A == '12c'), true).
test(scc_nondet_answers, setup_call_cleanup(true, member(X, [1, 2]), true), solutions([X = 1, X = 2])).
test(scc_cut, (with_output_to(atom(A), (setup_call_cleanup(true, member(X, [1, 2]), write(c)), !, write(X))), A == c1), true).
test(scc_cut_sees_goal, (setup_call_cleanup(true, member(X, [1, 2]), nb_setval(m11_scc_c, X)), !, nb_getval(m11_scc_c, V)), solutions([(X = 1, V = 1)])).
test(scc_once, (with_output_to(atom(A), (once(setup_call_cleanup(true, member(X, [1, 2]), write(c))), write(X))), A == c1), true).
test(scc_if_then, (with_output_to(atom(A), (setup_call_cleanup(true, member(X, [1, 2]), write(c)) -> write(X) ; true)), A == c1), true).
test(scc_not, (with_output_to(atom(A), (\+ \+ setup_call_cleanup(true, member(_, [1, 2]), write(c)), write(n))), A == cn), true).
test(scc_findall, (with_output_to(atom(A), findall(X, setup_call_cleanup(true, member(X, [1, 2]), write(c)), L)), A == c, L == [1, 2]), true).
test(scc_nested_cut, (with_output_to(atom(A), (setup_call_cleanup(true, setup_call_cleanup(true, member(_, [1, 2]), write(i)), write(o)), !)), A == io), true).
test(scc_cut_then_fail, (with_output_to(atom(A), (\+ (setup_call_cleanup(true, member(_, [1, 2]), write(c)), !, fail), write(alt))), A == calt), true).
test(scc_exception, (with_output_to(atom(A), catch(setup_call_cleanup(true, throw(e), write(c)), E, write(E))), A == ce), true).
test(scc_exception_bindings, (catch(setup_call_cleanup(X = s, (Y = g, throw(e)), nb_setval(m11_scc_e, X-Y)), e, true), nb_getval(m11_scc_e, P-Q), P == s, var(Q)), true).
test(scc_exception_wins, catch(setup_call_cleanup(true, throw(first), throw(second)), E, true), solutions([E = first])).
test(scc_exception_after_exit, (with_output_to(atom(A), catch((setup_call_cleanup(true, member(_, [1, 2]), write(c)), throw(late)), late, write(caught))), A == ccaught), true).
test(scc_exception_cleanup_fails, catch(setup_call_cleanup(true, throw(e), fail), E, true), solutions([E = e])).
test(scc_cleanup_throws_exit, catch(setup_call_cleanup(true, true, throw(oops)), B, true), solutions([B = oops])).
test(scc_cleanup_throws_fail, catch(setup_call_cleanup(true, fail, throw(oops)), B, true), solutions([B = oops])).
test(scc_cleanup_throws_cut, catch((setup_call_cleanup(true, member(_, [1, 2]), throw(oops)), !), B, true), solutions([B = oops])).
test(scc_cleanup_fails, setup_call_cleanup(true, true, fail), true).
test(scc_cleanup_nondet, setup_call_cleanup(true, true, member(Z, [1, 2])), solutions([Z = 1])).
test(scc_cleanup_cut_local, (setup_call_cleanup(true, true, (member(Z, [1, 2]), !)), Z == 1), true).
test(scc_setup_fails, (with_output_to(atom(A), (setup_call_cleanup(fail, write(g), write(c)) ; true)), A == ''), true).
test(scc_setup_throws, (with_output_to(atom(A), catch(setup_call_cleanup(throw(s), write(g), write(c)), s, true)), A == ''), true).
test(scc_setup_once, setup_call_cleanup(member(X, [1, 2]), true, true), solutions([X = 1])).
test(scc_setup_var, setup_call_cleanup(_, true, true), error(instantiation_error)).
test(scc_setup_type, setup_call_cleanup(1, true, true), error(type_error(callable, 1))).
test(scc_cleanup_var, setup_call_cleanup(true, true, _), error(instantiation_error)).
test(scc_cleanup_type, setup_call_cleanup(true, true, 1), error(type_error(callable, 1))).
test(scc_cleanup_after_setup, (with_output_to(atom(A), catch(setup_call_cleanup(write(s), write(g), 1), _, true)), A == s), true).
test(scc_cleanup_body, catch(setup_call_cleanup(true, true, (true, 1)), error(E, _), true), solutions([E = type_error(callable, (true, 1))])).
test(scc_goal_var, (with_output_to(atom(A), catch(setup_call_cleanup(true, _, write(c)), error(instantiation_error, _), write(e))), A == ce), true).
test(scc_goal_type, setup_call_cleanup(true, 1, true), error(type_error(callable, 1))).
test(scc_goal_cut_local, (setup_call_cleanup(true, (member(X, [1, 2]), !), true) ; X = 3), solutions([X = 1, X = 3])).
test(scc_det_loop, scc_loop_m11(200000), true).
test(scc_cut_loop, scc_cut_loop_m11(200000), true).
% 敵対的レビュー A1、A2、A4（M11）。節の本体のカットが外した後始末は、生成コードでも同じ歩みの続き（throw、算術、
% -> の then）より先に、カットの時点の束縛で動く。1 回のカットが外した後始末は、先のものが投げても全部動く。
test(scc_cut_then_throw, (catch(scc_cut_throw_m11, after(_), true), nb_getval(m11_scc_ct, V)), solutions([V = 1])).
test(scc_cut_then_eval_error, (catch(scc_cut_eval_m11, error(type_error(evaluable, foo/0), _), true), nb_getval(m11_scc_ce, V)), solutions([V = 1])).
test(scc_cut_then_throw_outer, (catch(setup_call_cleanup(true, scc_cut_throw_m11, nb_setval(m11_scc_co, outer)), after(_), true), nb_getval(m11_scc_ct, V), nb_getval(m11_scc_co, W)), solutions([(V = 1, W = outer)])).
test(scc_cut_then_arith, (scc_cut_arith_m11(Y), nb_getval(m11_scc_ca, V)), solutions([(Y = 2, V = unbound)])).
test(scc_commit_then_arith, (scc_commit_arith_m11(Y), nb_getval(m11_scc_ia, V)), solutions([(Y = 2, V = unbound)])).
test(scc_cut_two_cuts_arith, (scc_two_cuts_m11(Y), nb_getval(m11_scc_t1, V), nb_getval(m11_scc_t2, W)), solutions([(Y = 3, V = unbound, W = unbound)])).
test(scc_cut_rest_run, (nb_setval(m11_scc_two, none), catch((setup_call_cleanup(true, setup_call_cleanup(true, member(_, [1, 2]), throw(inner)), nb_setval(m11_scc_two, ran)), !), E, true), nb_getval(m11_scc_two, V)), solutions([(E = inner, V = ran)])).
test(scc_cut_first_wins, (with_output_to(atom(A), catch((setup_call_cleanup(true, setup_call_cleanup(true, member(_, [1, 2]), (write(i), throw(inner))), (write(o), throw(outer))), !), E, true)), A == io), solutions([E = inner])).
test(scc_cut_rest_run_body, (nb_setval(m11_scc_tb, none), catch(scc_cut_two_m11, E, true), nb_getval(m11_scc_tb, V)), solutions([(E = inner, V = ran)])).
% Codex レビュー（M11）。-> の確定と節の本体のカットの直後の throw/1 も、後始末の束縛を見てから投げる。
test(scc_commit_then_throw, (catch((setup_call_cleanup(true, (true ; true), X = ok) -> throw(X) ; true), E, true), E == ok), true).
test(scc_cut_then_throw_bound, (catch(scc_cut_throw_bound_m11, E, true), E == foo), true).
test(scc_commit_then_throw_body, (catch(scc_commit_throw_m11, E, true), E == foo), true).

scc_loop_m11(0) :- !.
scc_loop_m11(N) :- setup_call_cleanup(true, true, true), M is N - 1, scc_loop_m11(M).
scc_cut_loop_m11(0) :- !.
scc_cut_loop_m11(N) :- setup_call_cleanup(true, member(_, [a, b]), true), !, M is N - 1, scc_cut_loop_m11(M).
scc_cut_throw_m11 :- setup_call_cleanup(true, (X = 1 ; X = 2), nb_setval(m11_scc_ct, X)), !, throw(after([a, b, c, d, e, f])).
scc_cut_eval_m11 :- setup_call_cleanup(true, (X = 1 ; X = 2), nb_setval(m11_scc_ce, X)), !, _ is foo + X.
scc_cut_arith_m11(Y) :- setup_call_cleanup(true, (Z = 1 ; Z = 2), scc_note_m11(m11_scc_ca, Y)), !, Y is Z + 1.
scc_commit_arith_m11(Y) :- ( setup_call_cleanup(true, (Z = 1 ; Z = 2), scc_note_m11(m11_scc_ia, Y)) -> Y is Z + 1 ; Y = none ).
scc_two_cuts_m11(Y) :-
    setup_call_cleanup(true, (Z = 1 ; Z = 2), scc_note_m11(m11_scc_t1, Y)), !, W is Z + 1,
    setup_call_cleanup(true, (U = 1 ; U = 2), scc_note_m11(m11_scc_t2, Y)), !, Y is W + U.
scc_cut_two_m11 :- setup_call_cleanup(true, setup_call_cleanup(true, member(_, [1, 2]), throw(inner)), nb_setval(m11_scc_tb, ran)), !.
scc_cut_throw_bound_m11 :- setup_call_cleanup(true, (true ; true), X = foo), !, throw(X).
scc_commit_throw_m11 :- ( setup_call_cleanup(true, (true ; true), X = foo) -> throw(X) ; true ).
scc_note_m11(Key, X) :- ( var(X) -> nb_setval(Key, unbound) ; nb_setval(Key, X) ).
% レビュー L20。tab/1 の引数も式として評価する。
test(l20_tab_cyclic, (X = X+1, with_output_to(atom(_), tab(X))), error(resource_error(cyclic_term))).
test(l20_tab2_cyclic, (X = X+1, with_output_to(atom(_), (current_output(S), tab(S, X)))),
     error(resource_error(cyclic_term))).
