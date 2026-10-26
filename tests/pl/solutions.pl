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
test(bagof_exist_copy, (
  findall(Ls, bagof(L, A^B^C^(L = [A, B, C], bagof(t, (E = A; E = B; E = C), _)), Ls), All),
  All = [Ls],
  Ls = [[E, P, Q], [R, E, S], [T, U, E]],
  P \== E, Q \== E, R \== E, S \== E, T \== E, U \== E,
  P \== Q, P \== R, P \== S, P \== T, P \== U,
  Q \== R, Q \== S, Q \== T, Q \== U,
  R \== S, R \== T, R \== U,
  S \== T, S \== U,
  T \== U
), true).
test(bagof_free_alias, (
  findall(1, bagof(E, bagof(t, (A = E; B = E; C = E), _), _), N),
  N = [1],
  bagof(E, bagof(t, (A = E; B = E; C = E), _), L),
  (L == [A, B, C]; L == [C, B, A]),
  bagof(X, (Y = X; Z = X), S),
  S == [Y, Z]
), true).
test(setof_sort, setof(X, (X = b; X = a; X = b), L), solutions([L = [a, b]])).
test(setof_group, setof(X, d(X, Y), L), solutions([(Y = a, L = [1, 3]), (Y = b, L = [2])])).
test(setof_exist, setof(X, Y^d(X, Y), L), solutions([L = [1, 2, 3]])).

q(A, _, A).
q(_, A, A).
q(A, _, A).

:- dynamic(a/1).
a(1).
a(2).

:- dynamic(r/1).
r(1).
r(2).
r(3).

test(bagof_share, (bagof(f(X), (X = Y; X = Z), S), Y = a, S == [f(a), f(Z)]), true).
test(setof_same_var, (setof(X, (X = Y; X = Y), L), L == [Y]), true).
test(bagof_rename, bagof(none, q(X, Y, Z), L), solutions([(X = Z, L = [none, none]), (Y = Z, L = [none])])).
test(scan_asserta, (a(X), findall(_, asserta(a(9)), _)), solutions([X = 1, X = 2])).
test(a_now, a(X), solutions([X = 9, X = 9, X = 1, X = 2])).
test(scan_retract, (r(X), findall(_, (X = 1 -> retract(r(2)) ; true), _)), solutions([X = 1, X = 3])).
test(r_left, r(X), solutions([X = 1, X = 3])).
test(findall4_closed, findall(X, (X = 1; X = 2), L, []), solutions([L = [1, 2]])).
test(findall4_none, (findall(X, fail, L, T), L == T, var(T)), true).
test(findall4_open, (findall(X, (X = 1; X = 2), L, T), L == [1, 2|T], var(T)), true).
test(findall4_throw, findall(X, throw(error(instantiation_error, _)), L, T), error(instantiation_error)).
test(findall4_var_goal, findall(X, Y, L, T), error(instantiation_error)).
test(findall3_still, findall(X, (X = 1; X = 2), S), solutions([S = [1, 2]])).

:- dynamic(scan_p/1).
scan_p(a).
scan_p(b).
scan_p(c).

test(findall_throw_keeps_scan, (
  clause(scan_p(X), true),
  catch(findall(_, (asserta(scan_p(z)), throw(error(instantiation_error, _))), _, _),
        error(instantiation_error, _),
        true)
), solutions([X = a, X = b, X = c])).
