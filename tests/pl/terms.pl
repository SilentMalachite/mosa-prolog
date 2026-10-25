test(functor_out, functor(f(a, b), f, 2), true).
test(functor_in, functor(T, f, 2), solutions([T = f(_, _)])).
test(arg_1, arg(1, f(a, b), a), true).
test(arg_2, arg(2, f(a, b), b), true).
test(univ, (f(a, b) =.. [f, a, b]), true).
test(univ_make, (T =.. [f, a]), solutions([T = f(a)])).
test(copy_fresh, (copy_term(f(A), f(B)), A = 1, var(B)), true).
test(vars, term_variables(f(A, A, B), [A, B]), true).
