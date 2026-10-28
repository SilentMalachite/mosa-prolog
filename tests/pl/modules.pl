test(mod_qual_user, user:true, true).
test(mod_qual_unknown, nomod:true, error(existence_error(module, nomod))).
test(mod_qual_var, _:true, error(instantiation_error)).
test(mod_qual_type, 1:true, error(type_error(atom, 1))).
test(mod_qual_nested, user:(user:true, true), true).
mod_fact(1).
mod_fact(2).
test(mod_qual_user_pred, user:mod_fact(X), solutions([X = 1, X = 2])).
test(mod_qual_goal_var, user:_, error(instantiation_error)).
mod_cut_pick(X) :- (X = 1 ; X = 2), user:!.
test(mod_qual_cut, mod_cut_pick(X), solutions([X = 1])).
mod_cut_call(X) :- (X = 1 ; X = 2), call(user:!).
test(mod_qual_cut_call, mod_cut_call(X), solutions([X = 1, X = 2])).
test(mod_qual_system_unknown, system:mod_fact(_), error(existence_error(procedure, system:mod_fact/1))).
mod_pair(a, 1).
test(mod_qual_call_n, call(user:mod_pair, K, V), solutions([(K = a, V = 1)])).
test(mod_qual_call_n_nested, call(user:user:mod_pair(K), V), solutions([(K = a, V = 1)])).
test(mod_qual_call_n_var, call(user:_, a), error(instantiation_error)).
test(mod_qual_call_n_type, call(user:1, a), error(type_error(callable, user:1))).
test(mod_qual_call_n_module, call(nomod:mod_pair, _, _), error(existence_error(module, nomod))).
