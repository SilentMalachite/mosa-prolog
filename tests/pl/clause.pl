:- dynamic(insect/1).
insect(ant).
insect(bee).

:- dynamic(none/1).

legs(A, 6) :- insect(A).

path(A, C) :- edge(A, B), edge(B, C).

frozen(1).

quiet :- false.

test(clause_facts, clause(insect(X), true), solutions([X = ant, X = bee])).
test(clause_share, (clause(legs(A, 6), B), B == insect(A)), true).
test(clause_and, (clause(path(A, C), (edge(F, D), edge(E, G))), A == F, C == G, D == E), true).
test(clause_static, clause(frozen(X), B), solutions([(X = 1, B = true)])).
test(clause_false, clause(quiet, B), solutions([B = fail])).
test(clause_var, clause(X, Y), error(instantiation_error)).
test(clause_number, clause(1, Y), error(type_error(callable, 1))).
test(clause_body, clause(insect(X), 1), error(type_error(callable, 1))).
test(clause_builtin, clause(true, B), error(permission_error(access, private_procedure, true/0))).
test(clause_builtin_body, clause(true, 1), error(type_error(callable, 1))).
test(clause_missing, clause(no_such_pred_phase3(X), B), fail).
test(clause_empty, clause(none(X), B), fail).
test(clause_asserta, (asserta(insect(gnat)), clause(insect(X), true)), solutions([X = gnat, X = ant, X = bee])).
test(current_user, current_predicate(insect/1), true).
test(current_empty, current_predicate(none/1), true).
test(current_builtin, current_predicate(true/0), fail).
test(current_missing, current_predicate(no_such_pred_phase3/1), fail).
test(current_order, (assertz(zz1(a)), assertz(zz2(b)), findall(P, current_predicate(P), L), zz_pair(L)), true).
test(current_gen, current_predicate(N/0), solutions([N = quiet])).

zz_pair([zz1/1, zz2/1|_]).
zz_pair([zz1/1, zz2/1]).
zz_pair([_|T]) :- zz_pair(T).
