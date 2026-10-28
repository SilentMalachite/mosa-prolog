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
test(mod_qual_not_callable, user:1, error(type_error(callable, user:1))).
test(mod_qual_bad_body, user:(fail, 1), error(type_error(callable, user:(fail, 1)))).
test(mod_qual_bad_body_call, call(user:(fail, 1)), error(type_error(callable, user:(fail, 1)))).
:- consult('modules/家系.pl').
:- consult('modules/影.pl').
名前(利用者).
test(mod_call_export, 祖父(X, Z), solutions([(X = 太郎, Z = 花子), (X = 太郎, Z = 次郎)])).
test(mod_call_export_op, は(X, 太郎), solutions([X = 一郎])).
test(mod_call_private, 家系:内部(X), solutions([X = 秘密])).
test(mod_call_private_hidden, 内部(_), error(existence_error(procedure, 内部/1))).
test(mod_shadow, (影の名前(X), 名前(Y)), solutions([(X = 影, Y = 利用者)])).
test(mod_ctx_after_throw, (catch(家系:投げる, _, true), 名前(X)), solutions([X = 利用者])).
test(mod_ctx_after_redo, (家系:親(_, _), fail ; 名前(X)), solutions([X = 利用者])).
test(mod_export_late, 新述語(X), solutions([X = 1])).
:- consult('modules/全部.pl').
:- consult('modules/別名.pl').
:- consult('modules/除外.pl').
test(use_all, (全部:姓名分解(山田太郎, A, B), 全部:読み(A, R), 全部:敬称(A, C)), solutions([(A = 山田, B = 太郎, R = やまだ, C = 山田さん)])).
test(use_all_private, 全部:内緒(_), error(existence_error(procedure, 全部:内緒/1))).
test(use_alias, 別名:分解(山田太郎, A, B), solutions([(A = 山田, B = 太郎)])).
test(use_alias_hides, 別名:姓名分解(_, _, _), error(existence_error(procedure, 別名:姓名分解/3))).
test(use_alias_only, 別名:読み(_, _), error(existence_error(procedure, 別名:読み/2))).
test(use_except, (除外:姓名分解(山田太郎, A, _), 除外:敬称(A, C)), solutions([(A = 山田, C = 山田さん)])).
test(use_except_hides, 除外:読み(_, _), error(existence_error(procedure, 除外:読み/2))).
test(use_twice, findall(M, 人名:読込回数(M), L), solutions([L = [人名]])).
