test(true_ok, true, true).
test(fail_ok, fail, fail).
test(false_ok, false, fail).
test(cut_call, (call(!), fail ; true), true).
test(not_fail, (\+ fail), true).
test(not_true, (\+ true), fail).
test(once_choice, once((true ; fail)), true).
test(call_number, call(1), error(type_error(callable, 1))).
call_one(1).
test(iso_call_1_12, (call_one(One), call(One)), error(type_error(callable, 1))).
test(call_var, call(X), error(instantiation_error)).
test(catch_ball, catch(throw(a), a, true), true).
test(catch_miss, catch(throw(b), a, true), error(b)).
:- op(600, xfy, *->).
p(1).
p(2).
test(soft_keeps, (p(X) *-> true ; X = 9), solutions([X = 1, X = 2])).
test(soft_else, (fail *-> true ; true), true).
test(soft_fail, (fail *-> true), fail).
test(soft_cut, ((!, fail) *-> true ; true), true).
test(iso_conj_fail, (true, fail), fail).
test(iso_disj, (fail ; true), true).
test(iso_if_then, (true -> X = 1 ; X = 2), solutions([X = 1])).
test(iso_if_else, (fail -> X = 1 ; X = 2), solutions([X = 2])).
test(iso_not_conj, (\+ (true, fail)), true).
test(iso_once_fail, once(fail), fail).
test(iso_call_true, call(true), true).
test(iso_call_cut, (call(!), fail ; true), true).
test(iso_catch_bind, catch(throw(a), X, true), solutions([X = a])).
test(error_neg, throw(error(domain_error(not_less_than_zero, -1), _)), error(domain_error(not_less_than_zero, -1))).
test(throw_keeps_ball, catch((X = a, throw(f(X))), Y, Y == f(a)), true).
test(catch_after, (catch(true, _, true), throw(x)), error(x)).
test(if_cut_local, ((!, fail) -> true ; true), true).
test(var_goal_cut, (G = !, (G, fail ; true)), true).
test(call_conj_num, call((fail, 1)), error(type_error(callable, (fail, 1)))).
recover_cut :- catch(throw(x), _, !), fail.
recover_cut.
test(catch_recover_cut, recover_cut, true).
local_if :- (X = 1 ; X = 2), (! -> true), X = 2.
test(if_cut_keeps, local_if, true).
test(cut_not_not, findall(X, ((X=1;X=2), \+ \+ !), L), solutions([L = [1, 2]])).
