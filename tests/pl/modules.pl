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
:- consult('modules/複.pl').
:- dynamic(家系:動的/1).
test(mod_assert_qual, (assertz(家系:追加(a)), 家系:追加(X)), solutions([X = a])).
test(mod_assert_qual_hidden, (assertz(家系:隠す(a)), 隠す(_)), error(existence_error(procedure, 隠す/1))).
test(mod_assert_ctx, (家系:assertz(文脈(1)), 家系:文脈(X)), solutions([X = 1])).
test(mod_assert_ctx_hidden, (家系:assertz(文脈2(1)), 文脈2(_)), error(existence_error(procedure, 文脈2/1))).
test(mod_assert_rule_head, (assertz((家系:規則(X) :- X = 1)), 家系:規則(Y)), solutions([Y = 1])).
test(mod_assert_rule_body_ctx, (assertz(家系:(規則2(X) :- 内部(X))), 家系:規則2(Y)), solutions([Y = 秘密])).
test(mod_assert_unknown, assertz(nomod:p(1)), error(existence_error(module, nomod))).
test(mod_assert_var_module, assertz(_:p(1)), error(instantiation_error)).
test(mod_assert_static, assertz(家系:親(a, b)), error(permission_error(modify, static_procedure, 家系:親/2))).
test(mod_assert_imported_static, assertz(親(x, y)), error(permission_error(modify, static_procedure, 家系:親/2))).
test(mod_assert_imported_dynamic, (assertz(記録(1)), 家系:記録(X)), solutions([X = 1])).
test(mod_retract_qual, (assertz(家系:消す(1)), assertz(家系:消す(2)), retract(家系:消す(1)), findall(X, 家系:消す(X), L)), solutions([L = [2]])).
test(mod_retract_ctx, (家系:assertz(消す2(1)), 家系:retract(消す2(1)), findall(X, 家系:消す2(X), L)), solutions([L = []])).
test(mod_retract_imported, (assertz(記録(9)), retract(記録(9)), \+ 家系:記録(9)), true).
test(mod_retractall_qual, (assertz(家系:全消し(1)), retractall(家系:全消し(_)), findall(X, 家系:全消し(X), L)), solutions([L = []])).
test(mod_clause_qual, (assertz((家系:節本体(X) :- X = 2)), clause(家系:節本体(3), B)), solutions([B = (3 = 2)])).
test(mod_clause_qual_static, (clause(家系:祖父(_, _), B), B = (親(_, _), 親(_, _))), true).
test(mod_clause_unknown, clause(nomod:p(_), _), error(existence_error(module, nomod))).
test(mod_abolish_qual, (assertz(家系:捨てる(1)), abolish(家系:捨てる/1), 家系:捨てる(_)), error(existence_error(procedure, 家系:捨てる/1))).
test(mod_abolish_unknown, abolish(nomod:p/1), error(existence_error(module, nomod))).
test(mod_dynamic_qual, 家系:動的(_), fail).
test(mod_dynamic_qual_hidden, 動的(_), error(existence_error(procedure, 動的/1))).
test(mod_multifile, (家系:複(0), 家系:複(1)), true).
test(mod_current_predicate, current_predicate(家系:祖父/2), true).
test(mod_current_predicate_private, current_predicate(家系:内部/1), true).
test(mod_current_predicate_import, current_predicate(祖父/2), fail).
test(mod_current_predicate_ctx, 家系:current_predicate(内部/1), true).
test(mod_current_predicate_var_module, findall(M, current_predicate(M:祖父/2), L), solutions([L = [家系]])).
test(mod_assert_imported_undefined, (assertz(未定義(1)), findall(M, current_predicate(M:未定義/1), L)), solutions([L = [家系]])).
test(mod_abolish_imported_reassert, (abolish(記録/1), assertz(記録(1)), findall(M, current_predicate(M:記録/1), L)), solutions([L = [家系]])).
test(cm_known, (current_module(user), current_module(system), current_module(家系)), true).
test(cm_enum, findall(M, (current_module(M), M == 家系), L), solutions([L = [家系]])).
test(cm_unknown, current_module(nomod), fail).
test(cm_type, current_module(1), error(type_error(atom, 1))).
test(pp_builtin, predicate_property(atom_length(_, _), built_in), true).
test(pp_builtin_static, predicate_property(atom_length(_, _), static), true).
test(pp_builtin_not_dynamic, predicate_property(atom_length(_, _), dynamic), fail).
test(pp_control, predicate_property((_, _), built_in), true).
test(pp_imported, predicate_property(祖父(_, _), imported_from(家系)), true).
test(pp_exported, predicate_property(家系:祖父(_, _), exported), true).
test(pp_private, predicate_property(家系:内部(_), exported), fail).
test(pp_dynamic, predicate_property(家系:記録(_), dynamic), true).
test(pp_static, predicate_property(親(_, _), static), true).
test(pp_clauses, predicate_property(親(_, _), number_of_clauses(N)), solutions([N = 3])).
test(pp_multifile, predicate_property(家系:複(_), multifile), true).
test(pp_undefined, predicate_property(無い(_), defined), fail).
test(pp_enum, findall(P, predicate_property(家系:祖父(_, _), P), L), solutions([L = [defined, static, exported, number_of_clauses(1)]])).
test(pp_ctx, 家系:predicate_property(内部(_), defined), true).
test(pp_var, predicate_property(_, _), error(instantiation_error)).
test(pp_type, predicate_property(1, _), error(type_error(callable, 1))).
test(pp_domain, predicate_property(atom_chars(_, _), foobar), error(domain_error(predicate_property, foobar))).
test(pp_unknown_module, predicate_property(nomod:p, _), error(existence_error(module, nomod))).
test(meta_wrap, 全員(mod_fact, X), solutions([X = user:mod_fact])).
test(meta_wrap_ctx, 家系:全員(内部, X), solutions([X = 家系:内部])).
test(meta_already, 全員(家系:内部, X), solutions([X = 家系:内部])).
test(meta_private_closure, 全部秘密([秘密, 秘密]), true).
test(meta_findall_ctx, 家系:findall(X, 内部(X), L), solutions([L = [秘密]])).
test(meta_catch_ctx, 家系:catch(内部(X), _, true), solutions([X = 秘密])).
test(meta_call_n_ctx, 家系:call(内部, X), solutions([X = 秘密])).
test(pp_meta, predicate_property(家系:全員(_, _), meta_predicate(M)), solutions([M = 全員(1, ?)])).
test(pp_meta_imported, predicate_property(全員(_, _), meta_predicate(M)), solutions([M = 全員(1, ?)])).
test(pp_meta_none, predicate_property(親(_, _), meta_predicate(_)), fail).
test(pp_meta_builtin, predicate_property(findall(_, _, _), meta_predicate(M)), solutions([M = findall(?, 0, -)])).
test(forall_basic, forall(mod_fact(X), integer(X)), true).
test(forall_fail, forall(mod_fact(X), X == 1), fail).
test(forall_ctx, 家系:forall(内部(X), X == 秘密), true).
test(forall_meta, predicate_property(forall(_, _), meta_predicate(M)), solutions([M = forall(0, 0)])).
test(forall_lazy_action, forall(fail, 1), true).
test(forall_lazy_var, forall(fail, _), true).
test(forall_action_type, forall(true, 1), error(type_error(callable, 1))).
test(forall_cond_var, forall(_, true), error(instantiation_error)).
:- op(200, xfx, 全演).
:- op(700, xfx, 隠演).
:- consult('modules/演算.pl').
test(op_local, current_op(_, _, 内演), fail).
test(op_local_ctx, 演算:current_op(P, T, 内演), solutions([(P = 200, T = xfy)])).
test(op_local_read, 内演読み(X), solutions([X = 内演(a, b)])).
test(op_local_read_user, (open_string('a 内演 b. ', S), catch(read(S, _), error(syntax_error(_), _), true)), true).
test(op_local_read_ctx, (open_string('a 内演 b. ', S), 演算:read(S, T)), solutions([T = 内演(a, b)])).
test(op_exported, 所属例(X), solutions([X = 所属(太郎, 学生)])).
test(op_exported_read, X = (太郎 所属 学生), solutions([X = 所属(太郎, 学生)])).
test(op_user_global, 全演読み(X), solutions([X = 全演(a, b)])).
test(op_writeq_view, 内演書き(S), solutions([S = 'a 内演 b'])).
test(op_writeq_user, with_output_to(atom(S), writeq(内演(a, b))), solutions([S = '内演(a,b)'])).
test(op_writeq_ctx, 演算:with_output_to(atom(S), writeq(内演(a, b))), solutions([S = 'a 内演 b'])).
test(op_mask, 演算:current_op(_, xfx, 隠演), fail).
test(op_mask_user, current_op(700, xfx, 隠演), true).
test(op_runtime_local, (演算:op(300, xfx, 実演), \+ current_op(_, _, 実演), 演算:current_op(300, xfx, 実演)), true).
:- consult('modules/旗.pl').
test(flag_dq_inside_file, 旗の中の文字列(X), solutions([X = ab])).
test(flag_dq_per_file, current_prolog_flag(double_quotes, codes), true).
test(flag_jp_per_file, current_prolog_flag(japanese_punctuation, false), true).
test(flag_other_kept, current_prolog_flag(debug, on), true).
