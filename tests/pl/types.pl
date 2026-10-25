test(var_unbound, var(X), true).
test(var_atom, var(a), fail).
test(var_int, var(1), fail).
test(nonvar_atom, nonvar(a), true).
test(nonvar_var, nonvar(X), fail).
test(atom_nil, atom([]), true).
test(atom_a, atom(a), true).
test(atom_int, atom(1), fail).
test(integer_1, integer(1), true).
test(integer_float, integer(1.0), fail).
test(float_1, float(1.0), true).
test(float_int, float(1), fail).
test(atomic_nil, atomic([]), true).
test(atomic_list, atomic([a]), fail).
test(compound_nil, compound([]), fail).
test(compound_fun, compound(f(a)), true).
test(compound_list, compound([a]), true).
test(number_int, number(1), true).
test(number_float, number(1.0), true).
test(number_atom, number(a), fail).
test(callable_atom, callable(a), true).
test(callable_fun, callable(f(a)), true).
test(callable_number, callable(1), error(type_error(callable, 1))).
test(callable_var, callable(X), error(instantiation_error)).
test(list_proper, is_list([a]), true).
test(list_partial, is_list([a|b]), fail).
test(list_var, is_list(X), fail).
test(list_cycle, (X = [X], is_list(X)), fail).
