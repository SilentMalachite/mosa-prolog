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
test(missing, missing(1), error(existence_error(procedure, missing/1))).
test(iso_unify_both, ((X = Y, Y = 1)), solutions([(X = 1, Y = 1)])).
test(iso_unify_list, ([a, b] = [a, b]), true).
test(iso_unify_list_tail, ([a|T] = [a, b]), solutions([T = [b]])).
test(iso_not_unify_var, (X = 1, X \= 1), fail).
test(iso_occurs_list, unify_with_occurs_check(X, [X]), fail).
