d(1, a).
d(2, b).
d(3, a).

test(findall_two, findall(X, (X = 1; X = 2), S), solutions([S = [1, 2]])).
test(findall_none, findall(X, fail, S), solutions([S = []])).
test(findall_copy, (findall(f(X), (X = 1; X = 2), L), X = 3, L == [f(1), f(2)]), true).
test(findall_throw, findall(X, throw(error(instantiation_error, _)), S), error(instantiation_error)).
test(findall_var_goal, findall(X, Y, S), error(instantiation_error)).
test(bagof_none, bagof(X, fail, S), fail).
test(bagof_dup, bagof(X, (X = b; X = a; X = b), L), solutions([L = [b, a, b]])).
test(bagof_group, bagof(X, d(X, Y), L), solutions([(Y = a, L = [1, 3]), (Y = b, L = [2])])).
test(bagof_exist, bagof(X, Y^d(X, Y), L), solutions([L = [1, 2, 3]])).
test(setof_sort, setof(X, (X = b; X = a; X = b), L), solutions([L = [a, b]])).
test(setof_group, setof(X, d(X, Y), L), solutions([(Y = a, L = [1, 3]), (Y = b, L = [2])])).
test(setof_exist, setof(X, Y^d(X, Y), L), solutions([L = [1, 2, 3]])).
