test(unify_same, (1 = 1), true).
test(unify_bind, (X = 1), solutions([X = 1])).
test(unify_alias, (X = Y), solutions([X = Y])).
test(unify_anon, (_ = _), true).
test(unify_share, ((X = Y, X = abc)), solutions([(X = abc, Y = abc)])).
test(unify_struct, (f(X, def) = f(def, Y)), solutions([(X = def, Y = def)])).
test(unify_mismatch, (1 = 2), fail).
test(unify_int_float, (1 = 1.0), fail).
test(unify_diff_fun, (g(X) = f(f(X))), fail).
test(unify_arity, (f(X, 1) = f(a(X))), fail).
test(unify_cycle, (X = f(X)), true).
test(unify_list_cycle, (X = [X]), true).
test(cycle_tail, (X = [a|X]), solutions([X = [a|X]])).
test(named_tail, (X = [a|Y], Y = b), solutions([(X = [a|Y], Y = b)])).
test(not_unify, (1 \= 2), true).
test(not_unify_same, (1 \= 1), fail).
test(occurs_fail, unify_with_occurs_check(Y, f(Y)), fail).
test(occurs_ok, unify_with_occurs_check(Y, f(a)), solutions([Y = f(a)])).
% 8.2.2: 共有された部分項は循環ではない。
test(occurs_dag, (X = g(a), unify_with_occurs_check(_, f(X, X))), true).
test(missing, missing(1), error(existence_error(procedure, missing/1))).
test(iso_unify_both, ((X = Y, Y = 1)), solutions([(X = 1, Y = 1)])).
test(iso_unify_list, ([a, b] = [a, b]), true).
test(iso_unify_list_tail, ([a|T] = [a, b]), solutions([T = [b]])).
test(iso_not_unify_var, (X = 1, X \= 1), fail).
test(iso_occurs_list, unify_with_occurs_check(X, [X]), fail).
undo_clause :- X = f(A, b), X \= f(1, c), var(A).
test(unify_partial_undo, undo_clause, true).
occurs_local :- unify_with_occurs_check(X, f(X)).
test(occurs_local, occurs_local, fail).
occurs_pair :- unify_with_occurs_check(f(X, Y), f(Y, g(X))).
test(occurs_pair, occurs_pair, fail).

% レビュー L3。unify_with_occurs_check は、この単一化で束縛した変数の値が循環すれば失敗する（iso-notes 8.2.2 の決定 B）。
% 前から循環している項は、何も束縛せずに一致すれば成功する。前は結果が循環していれば失敗した。
test(l3_occurs_cyclic_equal, (X = f(X), Y = f(Y), unify_with_occurs_check(X, Y)), true).
test(l3_occurs_cyclic_unfolded, (X = f(X), Y = f(f(Y)), unify_with_occurs_check(X, Y)), true).
test(l3_occurs_cyclic_mismatch, (X = f(X), Y = f(g(Y)), unify_with_occurs_check(X, Y)), fail).
test(l3_occurs_bind_beside_old, (X = f(X), unify_with_occurs_check(g(X, A), g(X, B)), A == B), true).
test(l3_occurs_bind_inside_old, (X = f(X, A), unify_with_occurs_check(X, f(X, b)), A == b), true).
test(l3_occurs_bind_to_cyclic, (X = f(X), unify_with_occurs_check(A, X)), fail).
test(l3_occurs_bind_to_cyclic_list, (X = [a|X], unify_with_occurs_check(X, [a|Y])), fail).
test(l3_occurs_var_inside_cyclic, (X = f(X, A), unify_with_occurs_check(A, X)), fail).
test(l3_occurs_new_cycle_next_to_old, (X = f(X), unify_with_occurs_check(g(X, A), g(X, h(A)))), fail).
test(l3_occurs_through_earlier_binding, (X = f(X), unify_with_occurs_check(g(X, A, B), g(X, B, f(A)))), fail).
% 同じ方程式は、どの束縛を先に作っても同じ答えになる（L3 の敵対的レビュー A1）。
test(l3_occurs_order_left, (X = f(X), unify_with_occurs_check(g(A, A), g(X, f(A)))), fail).
test(l3_occurs_order_right, (X = f(X), unify_with_occurs_check(g(A, A), g(f(A), X))), fail).
