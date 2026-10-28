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
test(callable_number, callable(1), fail).
test(callable_var, callable(X), fail).
test(iso_callable_pair, callable((1, 2)), true).
test(list_proper, is_list([a]), true).
test(list_partial, is_list([a|b]), fail).
test(list_var, is_list(X), fail).
test(list_cycle, (X = [X], is_list(X)), fail).
test(iso_nonvar_compound, nonvar(f(X)), true).
test(iso_atomic_float, atomic(1.5), true).
test(iso_compound_fun, compound(f(X)), true).
test(iso_callable_nil, callable([]), true).
test(iso_callable_list, callable([a]), true).
test(iso_number_neg, integer(-3), true).
test(iso_float_neg_zero, float(-0.0), true).
% 8.3.10.4 の Examples（Cor.2）。
test(iso_ground_int, ground(3), true).
test(iso_ground_partial, ground(a(1, _)), fail).
test(ground_atom, ground(a), true).
test(ground_var, ground(f(_)), fail).
test(ground_nested, ground(f(g(a), [b])), true).
test(ground_list_tail, ground([a|_]), fail).
test(ground_deep, (numlist_deep(30000, T), ground(T)), true).
test(ground_deep_var, (numlist_deep(30000, T0), T = f(T0, _), ground(T)), fail).
% ground_deep は既定のヒープで作れる深さ。辿りは明示のスタックで、深さで C++ のスタックを使わない。
numlist_deep(N, T) :- deep_acc(N, a, T).
deep_acc(0, T, T) :- !.
deep_acc(N, Acc, T) :- M is N - 1, deep_acc(M, f(Acc), T).
