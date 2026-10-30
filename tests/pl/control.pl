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
% 実行時に項からゴールを組むとき、巡回する引数を切らない。切ると length/2 が部分リストと見て成功する。
test(cyclic_not, (L = [a, b|L], catch(\+ length(L, _), error(type_error(list, _), _), true)), true).
test(cyclic_once, (L = [a, b|L], catch(once(length(L, _)), error(type_error(list, _), _), true)), true).
test(cyclic_call_var, (L = [a, b|L], G = length(L, _), catch(G, error(type_error(list, _), _), true)), true).
test(cyclic_findall, (L = [a, b|L], catch(findall(N, length(L, N), _), error(type_error(list, _), _), true)), true).
test(cyclic_forall, (L = [a, b|L], catch(forall(true, length(L, _)), error(type_error(list, _), _), true)), true).
test(cyclic_catch_goal, (L = [a, b|L], G = catch(length(L, _), error(type_error(list, _), _), true), call(G)), true).
test(cyclic_not_not_eq, (L = [a|L], \+ \+ (L = [_|T], T == L)), true).

% iso-notes「Section 3 if/3」の決定 B。if(C, T, E) は (C *-> T ; E) と同じで、system の組込。
if_a(1).
if_a(2).
if_b(4).
if_b(5).
if_c(7).
if_c(8).
if_cut_cond :- if((!, fail), true, fail).
if_cut_cond.
if_cut_clause(1) :- if(!, true, fail).
if_cut_clause(2).
if_three(3).
test(if_then, if(true, true, fail), true).
test(if_else, if(fail, true, true), true).
test(if_then_fails, if(true, fail, fail), fail).
test(if_else_fails, if(fail, true, fail), fail).
test(if_all_cond, findall(X-Y, if(if_a(X), if_b(Y), if_c(_)), L), solutions([L = [1-4, 1-5, 2-4, 2-5]])).
test(if_all_else, findall(Z, if(fail, if_b(_), if_c(Z)), L), solutions([L = [7, 8]])).
test(if_cut_in_cond, findall(Z, if((!, fail), if_b(_), if_c(Z)), L), solutions([L = [7, 8]])).
test(if_cond_opaque, if_cut_cond, true).
test(if_cond_opaque_clause, findall(X, if_cut_clause(X), L), solutions([L = [1, 2]])).
test(if_then_cut_local, findall(X, (if_a(X), if(true, !, true)), L), solutions([L = [1, 2]])).
test(if_bad_cond, (if_three(T), if(T, true, true)), error(type_error(callable, if(3, true, true)))).
test(if_bad_then, (if_three(T), if(true, T, true)), error(type_error(callable, if(true, 3, true)))).
test(if_bad_else, (if_three(T), if(fail, true, T)), error(type_error(callable, if(fail, true, 3)))).
test(if_var_cond, if(_, true, true), error(instantiation_error)).
test(if_built_in, predicate_property(if(_, _, _), built_in), true).
% 7.8.9: if-then-else の then の中で投げた例外を捕まえ、回復のゴールが新しい制御を積んでも、then の制御が
% 回復の制御を上書きしない。
test(catch_then_recovery, catch((true -> throw(x) ; true), _, ((true -> true ; true), (true -> X = ok ; X = bad))), solutions([X = ok])).
test(catch_call_then_recovery, catch(call((true -> throw(x) ; true)), _, ((true *-> true ; true), (true -> X = ok ; X = bad))), solutions([X = ok])).
