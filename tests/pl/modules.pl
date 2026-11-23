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
% SPEC 第5節の宣言の例。家系.pl は例の宣言を全部持つ。
test(spec_example, 家系:祖父(太郎, Z), solutions([Z = 花子, Z = 次郎])).
test(spec_alias, 家系:分解(山田太郎, A, B), solutions([(A = 山田, B = 太郎)])).
test(spec_except, (家系:姓名分解(山田太郎, A, _), 家系:敬称(A, C)), solutions([(A = 山田, C = 山田さん)])).
test(spec_except_hides, 家系:読み(_, _), error(existence_error(procedure, 家系:読み/2))).
test(spec_lists, 家系:append([a], [b], L), solutions([L = [a, b]])).
test(spec_imports_hidden, 分解(_, _, _), error(existence_error(procedure, 分解/3))).
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
:- use_module('modules/別源.pl', [動/1 as 動き, 静/1 as 静か, 型/2 as 型別]).
% 別名を通したデータベース操作は取り込み元の述語に効く。罪人は取り込み元の名前（iso-notes 第5節 as の別名を通した節操作の決定 A）。
test(alias_db_clause, findall(X, clause(動き(X), true), L), solutions([L = [1, 2]])).
test(alias_db_assert, (assertz(動き(3)), findall(X-B, clause(別源:動(X), B), L)), solutions([L = [1-true, 2-true, 3-true]])).
test(alias_db_retract, findall(X, retract(動き(X)), L), solutions([L = [1, 2, 3]])).
test(alias_db_retractall, (assertz(動き(4)), assertz(動き(5)), retractall(動き(4)), findall(X, 別源:動(X), L)), solutions([L = [5]])).
test(alias_db_rule, (assertz((動き(6) :- true)), clause(別源:動(6), B)), solutions([B = true])).
test(alias_db_asserta, (asserta(動き(0)), once(別源:動(X))), solutions([X = 0])).
test(alias_db_static, assertz(静か(2)), error(permission_error(modify, static_procedure, 別源:静/1))).
test(alias_db_static_retract, retract(静か(1)), error(permission_error(modify, static_procedure, 別源:静/1))).
test(alias_pp_meta, predicate_property(型別(_, _), meta_predicate(M)), solutions([M = 型別(0, ?)])).
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
% モジュール固有の層への op/3 も、失敗で戻らない（レビュー M4、iso-notes 8.14.3 の決定 A）。
% 固有層の優先度 0 は消去ではなく印として残り、user の 戻演 を 演算 の中で隠す。戻演 はこのテストだけで使う。
test(m4_local_backtrack, ((演算:op(300, xfx, 戻演), fail ; true), 演算:current_op(300, xfx, 戻演), \+ current_op(_, _, 戻演), 演算:op(0, xfx, 戻演)), true).
% レビュー M-15（8.14.3.3）。同じ名前の中置と後置は、モジュールの局所の層と user の層にまたがっても置けない。置く順によらない。
test(m15_user_postfix_after_local, (演算:op(700, xfx, m15a), catch(op(200, xf, m15a), error(E, _), true), op(0, xf, m15a), 演算:op(0, xfx, m15a), E == permission_error(create, operator, m15a)), true).
test(m15_user_infix_after_local, (演算:op(200, xf, m15b), catch(op(700, xfx, m15b), error(E, _), true), op(0, xfx, m15b), 演算:op(0, xf, m15b), E == permission_error(create, operator, m15b)), true).
test(m15_local_infix_after_user, (op(200, xf, m15c), catch(演算:op(700, xfx, m15c), error(E, _), true), 演算:op(0, xfx, m15c), op(0, xf, m15c), E == permission_error(create, operator, m15c)), true).
% レビュー M-17（SPEC 第5節の規則 1）。system の述語の名前の別名は届かないので、ほかの取り込みの衝突と同じく
% permission_error(import_into, procedure, From:N/A) で断る（問いの既定 A）。ゴールの use_module は衝突で失敗する。
test(m17_alias_builtin, use_module(library(lists), [reverse/2 as atom_length]), fail).
test(m17_alias_control, use_module(library(lists), [reverse/2 as (=)]), fail).
test(m17_builtin_kept, (\+ use_module(library(lists), [reverse/2 as atom_length]), atom_length(abc, N), N == 3), true).
:- consult('modules/旗.pl').
test(flag_dq_inside_file, 旗の中の文字列(X), solutions([X = ab])).
test(flag_dq_per_file, current_prolog_flag(double_quotes, codes), true).
test(flag_jp_per_file, current_prolog_flag(japanese_punctuation, false), true).
test(flag_other_kept, current_prolog_flag(debug, on), true).
% iso-notes の Section 5 Autoload の決定 A。このファイルは library(lists) を取り込まない。
test(lists_needs_use, append([a], [b], _), error(existence_error(procedure, append/3))).
% 家系 が library(apply) を取り込んでも user には見えない。
test(apply_needs_use, maplist(integer, [1]), error(existence_error(procedure, maplist/2))).
test(format_needs_use, format("x", []), error(existence_error(procedure, format/2))).
test(strings_needs_use, atomic_list_concat([a], _), error(existence_error(procedure, atomic_list_concat/2))).
test(dcg_needs_use, expand_term(foo, _), error(existence_error(procedure, expand_term/2))).
test(japanese_needs_use, 読み幅(a, _), error(existence_error(procedure, 読み幅/2))).
% 試験器はテストのモジュールへ 6 ライブラリを取り込むが、mosa の user は変えない。
test(between_needs_use, between(1, 2, _), error(existence_error(procedure, between/3))).
% レビュー M28 の Codex レビュー。'.'/2 のメタ宣言は、読み手がリストにした頭でも読み込める。as の別名へ移した頭も、
% 別名から '.' へ移した頭も、メタ宣言の引数を保つ（Codex 敵対的レビュー）。
:- use_module('modules/m28_dot.pl', ['.'/2 as m28_q]).
:- consult('modules/m28_dot_back.pl').
test(m28_dot_meta, predicate_property(m28_dot:'.'(_, _), meta_predicate(M)), solutions([M = '.'(0, ?)])).
test(m28_dot_meta_call, m28_dot:'.'(X = 1, _), solutions([X = 1])).
test(m28_dot_alias_meta, predicate_property(m28_q(_, _), meta_predicate(M)), solutions([M = m28_q(0, ?)])).
test(m28_dot_alias_call, m28_q(X = 1, _), solutions([X = 1])).
test(m28_dot_back_meta, m28_back_meta(M), solutions([M = '.'(0, ?)])).
test(m28_dot_back_call, m28_back_call(X), solutions([X = 1])).

% 非終端記号の Name//Arity は Name/(Arity+2) の意味で書き出し・取り込みの表に書ける（ADR 0024 改訂 3）。
:- use_module('modules/dcg_export.pl', [dcg_item//1]).
test(mod_export_dcg_call, phrase(dcg_export:greet, [hello, world]), true).
test(mod_import_dcg_list, phrase(dcg_item(world), [world]), true).
test(mod_export_dcg_property, predicate_property(dcg_export:greet(_, _), exported), true).

% フェーズ12 Task 4（ADR 0026、SPEC 第5節）。use_module/1,2 と ensure_loaded/1 はゴールでも呼べる。ゴールの相対パスは
% 作業ディレクトリ（リポジトリの根）から開く。lists_needs_use が user に append/3 が無いことを確かめるので、user には
% lists を取り込まない。ライブラリを読むゴールは goal_ctx の中で呼ぶ。
:- consult('modules/goal_ctx.pl').
% goal_conflict.pl の公開する gx_c/1 と衝突させる、user の局所の述語。
gx_c(local).
test(goal_use_before, predicate_property(goal_ctx:last(_, _), imported_from(_)), fail).
test(goal_use_context, (goal_ctx:use_module(library(lists)), predicate_property(goal_ctx:last(_, _), imported_from(M)), goal_ctx:last([1, 2], X)), solutions([(M = lists, X = 2)])).
test(goal_use_det, findall(x, goal_ctx:use_module(library(lists)), L), solutions([L = [x]])).
test(goal_use_bound, (N = lists, goal_ctx:use_module(library(N), [last/2 as goal_last]), goal_ctx:goal_last([a], X)), solutions([X = a])).
test(goal_use_list, (use_module('tests/pl/modules/goal_pick.pl', [gp_a/1]), gp_a(X)), solutions([X = 1])).
test(goal_use_list_hidden, gp_b(_), error(existence_error(procedure, gp_b/1))).
test(goal_use_as, (use_module('tests/pl/modules/goal_pick.pl', [gp_b/1 as gp_bee]), gp_bee(X)), solutions([X = 2])).
% 空のリストは何も取り込まず、except(List) は List の外を取り込む（指令と同じ parse_imports。Muse レビュー Minor 11）。
% goal_ctx への取り込みは predicate_property の imported_from で見る（user の述語は imported_from に出ない）。
test(goal_use_empty, (goal_ctx:use_module('tests/pl/modules/goal_pick.pl', []), \+ predicate_property(goal_ctx:gp_b(_), imported_from(_))), true).
test(goal_use_except, (goal_ctx:use_module('tests/pl/modules/goal_pick.pl', except([gp_a/1])), predicate_property(goal_ctx:gp_b(_), imported_from(M)), \+ predicate_property(goal_ctx:gp_a(_), imported_from(_))), solutions([M = goal_pick])).
test(goal_use_flags, (current_prolog_flag(double_quotes, B), use_module('tests/pl/modules/goal_flags.pl'), current_prolog_flag(double_quotes, A), A == B), true).
test(goal_use_flags_module, (gf(X), atom(X)), true).
test(goal_ensure_loaded, (ensure_loaded('tests/pl/modules/goal_plain'), ensure_loaded('tests/pl/modules/goal_plain'), findall(X, gpl_fact(X), L)), solutions([L = [1]])).
% 失敗は、診断を user_error に 1 回書いて失敗する（問い 1 の A）。字面は tests/cpp/session_goals_test.cpp が見る。
test(goal_use_not_module, use_module('tests/pl/modules/goal_plain.pl'), fail).
test(goal_use_not_exported, use_module('tests/pl/modules/goal_pick.pl', [gp_hidden/0]), fail).
test(goal_use_missing, use_module('tests/tmp/p12_no_such.pl'), fail).
test(goal_use_lib_missing, use_module(library(p12_no_such)), fail).
% 読んだ後の衝突で失敗した読み込みは、モジュールも準備済みの印も残さない（ADR 0017）。直した取り込みで読み直せる。
test(goal_use_conflict, use_module('tests/pl/modules/goal_conflict.pl'), fail).
test(goal_use_conflict_left, current_module(goal_conflict), fail).
test(goal_use_conflict_retry, (use_module('tests/pl/modules/goal_conflict.pl', [gx_d/1]), gx_d(X)), solutions([X = 1])).
% 引数の誤りは、指令と同じ Formal を例外にする。
test(goal_use_var, use_module(_), error(instantiation_error)).
test(goal_use_type, use_module(1), error(type_error(atom, 1))).
test(goal_use_lib_var, use_module(library(_)), error(instantiation_error)).
test(goal_use_lib_type, use_module(library(1)), error(type_error(atom, 1))).
test(goal_use_imports_var, use_module(library(lists), _), error(instantiation_error)).
test(goal_use_imports_type, use_module(library(lists), foo), error(type_error(list, foo))).
test(goal_use_pi_type, use_module(library(lists), [foo]), error(type_error(predicate_indicator, foo))).
test(goal_use_alias_type, use_module(library(lists), [last/2 as 1]), error(type_error(atom, 1))).
% ゴールは引数を全部見てから探すので、Imports の誤りが無いライブラリより先に出る（consult.cpp の load_goal_problem）。
test(goal_use_imports_first, use_module(library(p12_no_such), foo), error(type_error(list, foo))).
test(goal_ensure_var, ensure_loaded(_), error(instantiation_error)).
test(goal_ensure_lib, ensure_loaded(library(lists)), error(type_error(atom, library(lists)))).
% 組み込みになったので、節を足せない。
test(goal_use_static, assertz(use_module(x)), error(permission_error(modify, static_procedure, use_module/1))).
test(goal_ensure_static, assertz(ensure_loaded(x)), error(permission_error(modify, static_procedure, ensure_loaded/1))).
% フェーズ12 Task 6（ADR 0026、SPEC 第5節）。reexport/1,2 は取り込んだ述語を読み込み中のモジュールの公開に足す。公開の
% 項目は定義したモジュールとそこでの名前を持つ。rx_except は rb_a/1 と ===> を rx_all に続く 2 つ目の道で取り込むので、
% この読み込みが通ることが、同じ定義に着く 2 つの取り込みを衝突にしないことの確かめになる。
:- use_module('modules/rx_all.pl').
:- use_module('modules/rx_some.pl').
:- use_module('modules/rx_except.pl').
test(rx_call, rb_a(X), solutions([X = 1])).
test(rx_call_b, rb_b(X), solutions([X = 2])).
test(rx_own, ra_own(X), solutions([X = 0])).
test(rx_imported_from, predicate_property(rb_a(_), imported_from(M)), solutions([M = rx_base])).
test(rx_alias, rs_a(X), solutions([X = 1])).
test(rx_alias_from, predicate_property(rs_a(_), imported_from(M)), solutions([M = rx_base])).
test(rx_op, (X = (a ===> b), X =.. L), solutions([L = [(===>), a, b]])).
% 同じ名前の違う述語は、今までどおり import_into の衝突になる。ゴールの use_module は診断を書いて失敗する。
test(rx_conflict, use_module('tests/pl/modules/rx_clash.pl'), fail).
test(rx_plain, (consult('tests/pl/modules/rx_plain.pl'), 残高(X)), solutions([X = 100])).
% 公開の項目の取り込み元が取り込む側のモジュール自身なら、それは自分の定義なので衝突でない（ADR 0026）。rx_base が
% rx_all を取り込むと、rx_all が再公開した rb_a/1 は rx_base 自身を指す。輪の rx_cyc_x は、節より先に取り込む。
test(rx_self, rx_base:use_module('tests/pl/modules/rx_all.pl'), true).
test(rx_self_call, rx_base:rb_a(X), solutions([X = 1])).
test(rx_cycle, (use_module('tests/pl/modules/rx_cyc_x.pl'), rcx_p(X)), solutions([X = 1])).
% 再公開の 2 段（rx_two → rx_all → rx_base）。取り込み元は定義したモジュールに着く（SPEC 第5節、Muse レビュー Minor 14）。
:- use_module('modules/rx_two_user.pl').
test(rx_two_exports, module_property(rx_two, exports(L)), solutions([L = [ra_own/1, rb_a/1, rb_b/1, (===>)/2]])).
test(rx_two_from, (predicate_property(rx_two_user:rb_b(_), imported_from(M)), predicate_property(rx_two_user:ra_own(_), imported_from(N))), solutions([(M = rx_base, N = rx_all)])).
test(rx_two_call, rx_two_user:rb_b(X), solutions([X = 2])).
% reexport は指令だけで、ゴールには無い。
test(rx_goal, reexport(rx_base), error(existence_error(procedure, reexport/1))).
test(rx_goal_2, reexport(rx_base, []), error(existence_error(procedure, reexport/2))).
% フェーズ12 Task 7（SPEC 第5節）。strip_module/3 は M: を M が原子である間だけ外し、無ければ文脈モジュールを返す。
% context_module/1 は文脈モジュール。ctx の節の中では ctx、meta_predicate の引数では呼び手のモジュールになる。
:- use_module('modules/ctx.pl').
test(sm_nested, strip_module(a:b:c(x), M, P), solutions([(M = b, P = c(x))])).
test(sm_plain, strip_module(foo, M, P), solutions([(M = user, P = foo)])).
test(sm_var_module, (strip_module(X:foo, M, P), P == X:foo), solutions([M = user])).
test(sm_number_module, strip_module(1:foo, M, P), solutions([(M = user, P = 1:foo)])).
test(sm_var_term, (strip_module(T, M, P), P == T), solutions([M = user])).
test(sm_check, strip_module(a:b, a, P), solutions([P = b])).
test(sm_check_fail, strip_module(a:b, c, _), fail).
test(sm_in_module, ctx_strip(x, R), solutions([R = ctx-x])).
test(sm_in_module_qualified, ctx_strip(q:x, R), solutions([R = q-x])).
test(cm_user, context_module(M), solutions([M = user])).
test(cm_module, ctx_here(M), solutions([M = ctx])).
test(cm_meta, ctx_call(context_module(M)), solutions([M = user])).
test(cm_qualified, ctx:context_module(M), solutions([M = ctx])).
% 組み込みになったので、節を足せない。
test(sm_static, assertz(strip_module(a, b, c)), error(permission_error(modify, static_procedure, strip_module/3))).
test(cm_static, assertz(context_module(a)), error(permission_error(modify, static_procedure, context_module/1))).
% フェーズ12 Task 8（SPEC 第5節）。module_property/2 の性質は file(F)、exports(L)、exported_operators(L) の順。
% module/2 で作ったモジュールだけが性質を持つ。exports は再公開の項目も公開した順に並べる。
% import/1 は公開述語 1 つを文脈モジュールへ取り込む。M: が無ければ自分からの取り込みで、何も変えない。
:- consult('modules/imp_ctx.pl').
test(mp_file, (module_property(rx_base, file(F)), sub_atom(F, _, _, 0, 'rx_base.pl')), true).
test(mp_exports, module_property(rx_all, exports(L)), solutions([L = [ra_own/1, rb_a/1, rb_b/1, (===>)/2]])).
test(mp_exports_alias, module_property(rx_some, exports(L)), solutions([L = [rs_a/1]])).
test(mp_exports_except, module_property(rx_except, exports(L)), solutions([L = [rb_a/1, (===>)/2]])).
test(mp_ops, module_property(rx_base, exported_operators(L)), solutions([L = [op(700, xfx, ===>)]])).
test(mp_ops_reexported, module_property(rx_all, exported_operators(L)), solutions([L = [op(700, xfx, ===>)]])).
test(mp_ops_none, module_property(rx_some, exported_operators(_)), fail).
test(mp_order, (findall(P, module_property(rx_some, P), [file(_), exports([rs_a/1])])), true).
test(mp_user, module_property(user, _), fail).
test(mp_system, module_property(system, _), fail).
% user は lists を取り込まないので、ライブラリの述語を使わない。
test(mp_enum, findall(M, (module_property(M, exports(_)), (M == rx_all ; M == user)), Ms), solutions([Ms = [rx_all]])).
% 誤りは M の型、P の領域、M があるか、の順に確かめる。
test(mp_type, module_property(1, _), error(type_error(atom, 1))).
test(mp_domain, module_property(user, foo), error(domain_error(module_property, foo))).
test(mp_domain_first, module_property(nomod, foo), error(domain_error(module_property, foo))).
test(mp_no_module, module_property(nomod, _), error(existence_error(module, nomod))).
test(imp_basic, (imp_ctx:import(rx_base:rb_b/1), imp_ctx:rb_b(X)), solutions([X = 2])).
test(imp_reexported, (imp_ctx:import(rx_some:rs_a/1), predicate_property(imp_ctx:rs_a(_), imported_from(M))), solutions([M = rx_base])).
% M1:M2:PI は内側の M2 から取り込む。外側の foo はモジュールでなくてよい。imp_basic がまだ取り込んでいない ===> で、
% 取り込みが起きたことを見る（Muse レビュー Minor 10）。
test(imp_nested, (imp_ctx:import(foo:rx_base:(===>)/2), predicate_property(imp_ctx:'===>'(_, _), imported_from(M))), solutions([M = rx_base])).
% M: が無ければ自分からの取り込みで、何も変えない。取り込み済みの述語の取り込み元も、無い述語も、そのまま。
test(imp_unqualified, (import(rb_a/1), predicate_property(rb_a(_), imported_from(M))), solutions([M = rx_base])).
test(imp_unqualified_none, (import(p12_none/3), \+ predicate_property(p12_none(_, _, _), _)), true).
test(imp_var, import(_), error(instantiation_error)).
test(imp_module_var, import(_:p/1), error(instantiation_error)).
test(imp_module_type, import(1:p/1), error(type_error(atom, 1))).
test(imp_no_module, import(nomod:p/1), error(existence_error(module, nomod))).
test(imp_pi_var, import(rx_base:_), error(instantiation_error)).
test(imp_pi_type, import(rx_base:foo), error(type_error(predicate_indicator, foo))).
% PI の名前と引数の数の誤りは use_module/2 の項目と同じ read_indicator が出す（Muse レビュー Minor 12）。
test(imp_name_type, import(rx_base:(1/2)), error(type_error(atom, 1))).
test(imp_arity_type, import(rx_base:(p/x)), error(type_error(integer, x))).
test(imp_arity_negative, import(rx_base:(p/(-1))), error(domain_error(not_less_than_zero, -1))).
test(imp_arity_max, import(rx_base:(p/1025)), error(representation_error(max_arity))).
test(imp_as, import(rx_base:(rb_a/1 as x)), error(type_error(predicate_indicator, rb_a/1 as x))).
test(imp_not_exported, import(rx_base:nope/0), error(existence_error(procedure, rx_base:nope/0))).
% imp_ctx は自分の rb_a/1 を持つ。
test(imp_conflict, imp_ctx:import(rx_base:rb_a/1), error(permission_error(import_into, procedure, rx_base:rb_a/1))).
% 組み込みになったので、節を足せない。
test(mp_static, assertz(module_property(a, b)), error(permission_error(modify, static_procedure, module_property/2))).
test(imp_static, assertz(import(a)), error(permission_error(modify, static_procedure, import/1))).
% 循環する M: の連なり（7.3.3 は未定義）は resource_error(cyclic_term) を投げて止まる（iso-notes 7.3.3、持ち主の決定 A、
% 2026-11-17）。前は何も割り当てずに回り続け、--heap-max でも止まらなかった。周期 1 と 2、周期の前に層のある形を見る。
test(cyc_sm_self, (X = a:X, strip_module(X, _, _)), error(resource_error(cyclic_term))).
test(cyc_sm_two, (X = a:Y, Y = b:X, strip_module(X, _, _)), error(resource_error(cyclic_term))).
test(cyc_sm_tail, (X = a:b:c:Y, Y = d:e:f:Y, strip_module(X, _, _)), error(resource_error(cyclic_term))).
% 循環しない長い連なりは、今までどおり一番内側を返す。2,000 層で見張りの窓は 10 回動く。100,000 層にすると、
% 連なりを作る節が GC ストレス（MOSA_GC_STRESS=1）の下で深さの 2 乗の時間を食い、このファイルが 265 秒かかった（Opus レビュー A1）。
cyc_chain(0, T, T) :- !.
cyc_chain(N, T0, T) :- N1 is N - 1, cyc_chain(N1, m:T0, T).
test(cyc_sm_long, (cyc_chain(2000, x, T), strip_module(T, M, P)), solutions([(M = m, P = x)])).
% 節操作と述語の性質は target.cpp の strip_module を通る。各層の検査が先なので、無いモジュールはその誤りになる。
test(cyc_assertz, (X = user:X, assertz(X)), error(resource_error(cyclic_term))).
test(cyc_asserta_head, (X = user:X, asserta((X :- true))), error(resource_error(cyclic_term))).
test(cyc_retract, (X = user:X, retract(X)), error(resource_error(cyclic_term))).
test(cyc_retractall, (X = user:X, retractall(X)), error(resource_error(cyclic_term))).
test(cyc_clause, (X = user:X, clause(X, _)), error(resource_error(cyclic_term))).
test(cyc_abolish, (X = user:X, abolish(X)), error(resource_error(cyclic_term))).
test(cyc_pred_prop, (X = user:X, predicate_property(X, _)), error(resource_error(cyclic_term))).
test(cyc_no_module_first, (X = nomod:X, assertz(X)), error(existence_error(module, nomod))).
test(cyc_assertz_two_tail, (Y = user:user:Y, X = user:Y, assertz(X)), error(resource_error(cyclic_term))).
test(cyc_no_module_in_cycle, (Y = user:nomod:Y, X = user:Y, assertz(X)), error(existence_error(module, nomod))).
test(cyc_retract_head, (X = user:X, retract((X :- true))), error(resource_error(cyclic_term))).
test(cyc_pred_prop_no_module, (X = nomod:X, predicate_property(X, _)), error(existence_error(module, nomod))).
test(cyc_import, (X = a:X, import(X)), error(resource_error(cyclic_term))).
test(cyc_import_two, (X = a:Y, Y = b:X, import(X)), error(resource_error(cyclic_term))).
test(cyc_import_var_first, (X = a:_:X, import(X)), error(instantiation_error)).
% レビュー H-10。:- meta_predicate の頭は、「,」でつないでもリストに入れても、左から右の順に宣言する。同じ述語を
% 2 度宣言すると後の宣言が残るので、残った形で順が分かる（手で積む collect_heads が子を積む順を見る）。
:- meta_predicate (h10_c(0), h10_c(:)).
:- meta_predicate [h10_l(:), h10_l(0)].
:- meta_predicate (([h10_m(0)], h10_m(^)), [h10_m(:)]).
h10_c(G) :- call(G).
h10_l(G) :- call(G).
h10_m(G) :- call(G).
test(h10_order_comma, predicate_property(h10_c(_), meta_predicate(M)), solutions([M = h10_c(:)])).
test(h10_order_list, predicate_property(h10_l(_), meta_predicate(M)), solutions([M = h10_l(0)])).
test(h10_order_mixed, predicate_property(h10_m(_), meta_predicate(M)), solutions([M = h10_m(:)])).

% レビュー H-12（2026-11-21、持ち主の B）。名前が user か system の module/2 は permission_error(create, module, Name)
% で読み込みを失敗にする（Formal は tests/cpp/module_load_test.cpp が診断の文で見る）。前は user の取り込みを読み直しと
% 同じくすべて消し（その後の append/3 が existence_error）、system は利用者の述語をどのモジュールからも呼べるようにした。
h12_load(Name, Text) :-
    atom_concat('tests/tmp/h12_', Name, Base), atom_concat(Base, '.pl', Path),
    open(Path, write, S), write(S, Text), close(S), consult(Path).
test(h12_user,
     (use_module(library(lists)), \+ h12_load(user, ':- module(user, [h12_p/0]).\nh12_p.\n'),
      append([a], [b], L), L == [a, b], \+ current_predicate(h12_p/0)), true).
test(h12_system,
     (\+ h12_load(system, ':- module(system, [h12_q/0]).\nh12_q.\n'),
      catch(h12_q, error(existence_error(procedure, _), _), true)), true).

% レビュー L-24。system には利用者の述語を作らない。assert も、ファイルの system: の節も、system: の dynamic 宣言も
% permission_error(modify, static_procedure, system:Name/Arity) で断る。前は作り、既定の鎖（user → system）で
% どのモジュールからも呼べた。
test(l24_assertz_system, assertz(system:l24_foo), error(permission_error(modify, static_procedure, system:l24_foo/0))).
test(l24_asserta_system, asserta(system:(l24_foo :- true)), error(permission_error(modify, static_procedure, system:l24_foo/0))).
test(l24_not_created, (catch(assertz(system:l24_bar), _, true), \+ predicate_property(system:l24_bar, defined)), true).
test(l24_clause_system,
     (\+ h12_load(l24_clause, 'system:l24_baz.\n'), \+ predicate_property(system:l24_baz, defined)), true).
test(l24_dynamic_system,
     (\+ h12_load(l24_dynamic, ':- dynamic(system:l24_qux/0).\n'), \+ predicate_property(system:l24_qux, defined)), true).
test(l24_meta_system,
     (\+ h12_load(l24_meta, ':- meta_predicate(system:l24_m(0)).\n'), \+ predicate_property(system:l24_m(_), defined)),
     true).
