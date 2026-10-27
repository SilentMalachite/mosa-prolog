test(functor_out, functor(f(a, b), f, 2), true).
test(functor_in, functor(T, f, 2), solutions([T = f(_, _)])).
test(arg_1, arg(1, f(a, b), a), true).
test(arg_2, arg(2, f(a, b), b), true).
test(univ, (f(a, b) =.. [f, a, b]), true).
test(univ_make, (T =.. [f, a]), solutions([T = f(a)])).
test(copy_fresh, (copy_term(f(A), f(B)), A = 1, var(B)), true).
test(vars, term_variables(f(A, A, B), [A, B]), true).
test(iso_functor_3, functor(foo(a, b, c), foo, 3), true).
test(iso_functor_nil, functor([], [], 0), true).
test(iso_functor_list, functor([a, b], '.', 2), true).
test(iso_functor_number, functor(1, X, Y), solutions([(X = 1, Y = 0)])).
test(iso_functor_make, functor(X, foo, 0), solutions([X = foo])).
test(iso_functor_make3, functor(X, foo, 3), solutions([X = foo(_, _, _)])).
test(iso_functor_dot, (functor(T, '.', 2), arg(1, T, a), arg(2, T, [])), solutions([T = [a]])).
test(iso_arg_1, arg(1, foo(a, b), a), true).
test(iso_arg_2, arg(2, foo(a, b), X), solutions([X = b])).
test(iso_arg_0, arg(0, foo(a), X), fail).
test(iso_univ_out, (foo(a, b) =.. L), solutions([L = [foo, a, b]])).
test(iso_univ_in, (X =.. [foo, a]), solutions([X = foo(a)])).
test(iso_univ_list, (T =.. ['.', a, []]), solutions([T = [a]])).
test(iso_univ_list_unify, (T =.. ['.', a, []], T = [a]), true).
test(iso_univ_list_eq, (T =.. ['.', a, []], T == [a]), true).
test(iso_univ_list_is_list, (T =.. ['.', a, []], is_list(T)), true).
test(iso_dot_unify, ('.'(a, []) = [a]), true).
test(iso_dot_eq, ('.'(a, []) == [a]), true).
test(iso_dot_list, is_list('.'(a, [])), true).
test(iso_univ_atom, (foo =.. L), solutions([L = [foo]])).
test(iso_univ_int, (1 =.. L), solutions([L = [1]])).
test(iso_univ_empty, (X =.. []), error(domain_error(non_empty_list, []))).
test(iso_univ_number_head, (X =.. [1, 2]), error(type_error(atom, 1))).
test(iso_univ_compound_head, (X =.. [a(b), 1]), error(type_error(atom, a(b)))).
test(iso_univ_compound_one, (X =.. [a(b)]), error(type_error(atomic, a(b)))).
test(iso_copy_ground, copy_term(f(a), X), solutions([X = f(a)])).
test(iso_vars_empty, term_variables(a, L), solutions([L = []])).
test(arity_limit, functor(T, foo, 1025), error(representation_error(max_arity))).
test(vars_improper, term_variables(t, [a|b]), error(type_error(list, [a|b]))).
