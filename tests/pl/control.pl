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
% ADR 0012: 決定的に抜けた catch/3 と *-> の Entry は外す。選択点が残るときと外したときで、見える動きが変わらないこと。
test(catch_redo, catch(p(X), _, true), solutions([X = 1, X = 2])).
test(catch_redo_after, catch((catch(p(X), _, fail), X == 2, throw(t)), t, true), true).
test(catch_exit_nested, catch((catch(true, _, fail), throw(b)), b, true), true).
cc_cut :- catch(true, _, true), !, fail.
cc_cut.
test(catch_exit_cut, cc_cut, fail).
sc_cut :- (true *-> true ; true), !, fail.
sc_cut.
test(soft_exit_cut, sc_cut, fail).
test(soft_exit_outer, (findall(X-Y, (p(X), (true *-> (Y = 1) ; Y = 2)), L), L == [1-1, 2-1]), true).
test(soft_in_catch, catch((true *-> (X = 1) ; X = 2), _, true), solutions([X = 1])).
test(soft_redo_cond, findall(X, (p(X) *-> true ; X = 0), [1, 2]), true).
% 外した Entry は consult の「実行中」にならない。op/3 はどの抜け方の後でも、失敗で戻らない（iso-notes 8.14.3 の決定 A）。
op_kept(Op, K) :- ( current_op(_, _, Op) -> K = kept ; K = undone ).
test(op_exit_same, ((consult('tests/pl/modules/op_exit.pl'), fail ; true), op_kept('=once=>', A), op_kept('=catch=>', B), op_kept('=soft=>', C), op_kept('=ite=>', D), op(0, xfx, ['=once=>', '=catch=>', '=soft=>', '=ite=>']), [A, B, C, D] == [kept, kept, kept, kept]), true).
test(soft_exit_consult, (consult('tests/pl/modules/redef_a.pl'), (true *-> true ; true), consult('tests/pl/modules/redef_b.pl'), findall(X, redef(X), L), L == [2]), true).
% 指令の実行は選択点スタックを退避して空から積む。その中で抜けた catch の記録が、外の同じ位置の抜けた
% catch を、外の選択点へ戻ったときに有効に戻してはいけない（ADR 0011 の改訂 3）。
test(catch_parked_rearm,
     (catch(true, _, fail), catch(true, _, fail), (X = 1 ; X = 2),
      (X == 1 -> consult('tests/pl/catch_park_dir.pl') ; true), X == 2, throw(boom)),
     error(boom)).
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

% ADR 0014（レビュー M14）: catch/3 の Goal と Recovery は catch が動くときに call/1 として訳す（7.8.9.1）。
% ゴール位置の変数がそのとき ! なら、そのゴールに閉じたカットになる。call(X) の壁にしてはいけない。
:- dynamic(m14_seen/0).
:- dynamic(m14_dyn/1).
m14_m(1).
m14_m(2).
m14_m(3).
m14_goal(X) :- catch(((X, fail) ; true), _, true).
m14_recovery(X) :- catch(throw(e), _, ((X, fail) ; true)).
m14_prefix(X, Y) :- catch((m14_m(Y), X), _, true).
m14_else(X, Y) :- catch((m14_m(Y), (fail -> true ; X)), _, true).
m14_soft(X, Y) :- catch((m14_m(Y), (true *-> X ; true)), _, true).
m14_outer(X, Z) :- m14_m(Z), Z < 3, catch(((X, true) ; true), _, true).
m14_clause(X, Z) :- catch((X ; true), _, true), Z = a.
m14_clause(_, b).
m14_inner(X, Y) :- catch(catch((m14_m(Y), X), _, true), _, true).
m14_lone(X, Y) :- catch((m14_m(Y), catch(X, _, true)), _, true).
m14_rec_prefix(X, Y) :- catch(throw(e), _, (m14_m(Y), X)).
m14_ball(Y) :- catch(throw(!), X, (m14_m(Y), X)).
m14_late :- catch((X = !, ((X, fail) ; true)), _, true).
m14_unbound(E) :- catch(((_, fail) ; true), E, true).
m14_undone(Y) :- catch((X = !, throw(e)), _, (m14_m(Y), X)).
m14_noncallable(G, E) :- catch((G, 1), E, true).
m14_not(X) :- \+ ((X, fail) ; true).
m14_qual(X) :- user:((X, fail) ; true).
m14_qual(_).
m14_not_qual(X) :- \+ user:((X, fail) ; true).
m14_ite(X, Y) :- catch((X ; Y = 9), _, true).
m14_not_catch(X) :- \+ catch(((X, fail) ; true), _, true).
m14_not_arg(X) :- \+ (m14_m(X), X > 1).
m14_written(X) :- catch(call(((X, fail) ; true)), _, true).
test(m14_goal_cut, m14_goal(!), fail).
test(m14_goal_same_as_call, (G = catch(((X, fail) ; true), _, true), X = !, \+ call(G), \+ m14_goal(!)), true).
test(m14_goal_true, m14_goal(true), true).
test(m14_recovery_cut, m14_recovery(!), fail).
test(m14_runtime_cut, call((Y = !, catch(((Y, fail) ; true), _, true))), fail).
test(m14_prefix_cut, (findall(Y, m14_prefix(!, Y), L), L == [1]), true).
test(m14_prefix_conj_cut, (findall(Y, m14_prefix((!, true), Y), L), L == [1]), true).
test(m14_prefix_true, (findall(Y, m14_prefix(true, Y), L), L == [1, 2, 3]), true).
% findall の中の catch は X が束縛された後に訳される。節の catch と同じ答えになること。
test(m14_prefix_runtime, (X = !, findall(Y, catch((m14_m(Y), X), _, true), L)), solutions([L = [1]])).
test(m14_else_cut, (findall(Y, m14_else(!, Y), L), L == [1]), true).
test(m14_soft_cut, (findall(Y, m14_soft(!, Y), L), L == [1]), true).
test(m14_cut_stays_in_catch, (findall(Z, m14_outer(!, Z), L), L == [1, 2]), true).
test(m14_cut_not_clause, findall(Z, m14_clause(!, Z), L), solutions([L = [a, b]])).
test(m14_inner_catch, (findall(Y, m14_inner(!, Y), L), L == [1]), true).
test(m14_lone_var, (findall(Y, m14_lone(!, Y), L), L == [1, 2, 3]), true).
test(m14_recovery_prefix, (findall(Y, m14_rec_prefix(!, Y), L), L == [1]), true).
% 捕まえる型との単一化で X が ! になる。Recovery はその後に訳す。
test(m14_recovery_ball, (findall(Y, m14_ball(Y), L), L == [1]), true).
test(m14_recovery_ball_runtime, findall(Y, catch(throw(!), X, (m14_m(Y), X)), L), solutions([L = [1]])).
test(m14_bound_late, m14_late, true).
test(m14_bound_late_call, catch((X = !, ((X, fail) ; true)), _, true), true).
test(m14_unbound, (m14_unbound(E), E = error(F, _)), solutions([F = instantiation_error])).
test(m14_recovery_undone, m14_undone(_), error(instantiation_error)).
test(m14_noncallable_first, (m14_noncallable(assertz(m14_seen), E), E = error(F, _), \+ m14_seen), solutions([F = type_error(callable, (assertz(m14_seen), 1))])).
test(m14_noncallable_call, (catch(call(catch((X, 1), E, true)), O, true), var(O), E = error(type_error(callable, (V, 1)), _), var(V)), true).
test(m14_throw_through, catch(m14_noncallable_rethrow, b, true), true).
m14_noncallable_rethrow :- G = throw(b), catch((G ; true), a, true).
test(m14_throw_caught, (G = throw(z), catch((G ; true), z, true)), true).
test(m14_not_cut, m14_not(!), true).
test(m14_qual_cut, findall(a, m14_qual(!), L), solutions([L = []])).
test(m14_not_qual_cut, m14_not_qual(!), true).
% X が -> になると (X ; E) は if-then-else になり、else の枝は動かない（7.8.8）。
test(m14_ite_shape, (findall(Y, (X = (true -> Y = 1), m14_ite(X, Y)), L), L == [1]), true).
test(m14_not_catch_cut, m14_not_catch(!), true).
test(m14_not_arg, (m14_not_arg(1), \+ m14_not_arg(2)), true).
test(m14_clause_shown, (clause(m14_goal(_), B), B = catch(G, _, true), G = ((V, fail) ; true), var(V)), true).
test(m14_clause_recovery_shown, (clause(m14_recovery(_), B), B = catch(throw(e), _, R), R = ((V, fail) ; true), var(V)), true).
test(m14_clause_same_as_head, (clause(m14_goal(!), B), \+ call(B)), true).
% レビュー M15。変数のない catch/3 の Goal と Recovery も call/1 と同じく、呼べない部分があれば何も動かす前に
% type_error(callable, Goal) になる（7.8.9.1、7.8.3.3）。catch/3 自体は呼べる項なので、外の call/1 や
% assertz/1 は中を見ない（7.6.2 は catch/3 の引数を変換しない）。
:- dynamic(m15_seen/0).
:- dynamic(m15_dyn/1).
m15_goal(E) :- catch((assertz(m15_seen), 1), E, true).
m15_recovery :- catch(throw(x), _, (assertz(m15_seen), 1)).
m15_lone_recovery :- catch(throw(x), _, 1).
m15_disj(L) :- findall(E, catch((true ; 1), E, true), L).
m15_ite(E) :- catch((fail -> true ; (true, 1)), E, true).
m15_float(E) :- catch((true, 1.5), E, true).
m15_big(E) :- catch((true, 9223372036854775807), E, true).
m15_lone(E) :- catch(1, E, true).
m15_nested(I, O) :- catch(catch((true, 1), I, true), O, true).
m15_throw_first(E) :- catch((throw(a), 1), E, true).
m15_cut_first(L) :- findall(E, catch((m14_m(_), !, 1), E, true), L).
m15_outside(E) :- catch((assertz(m15_seen), catch(1, _, true), 2), E, true).
m15_formal(E, F) :- E = error(F, _).
test(m15_goal_first, (retractall(m15_seen), m15_goal(E), m15_formal(E, F), \+ m15_seen), solutions([F = type_error(callable, (assertz(m15_seen), 1))])).
test(m15_recovery_first, (retractall(m15_seen), catch(m15_recovery, E, true), m15_formal(E, F), \+ m15_seen), solutions([F = type_error(callable, (assertz(m15_seen), 1))])).
test(m15_lone_recovery, m15_lone_recovery, error(type_error(callable, 1))).
test(m15_disj_once, (m15_disj(L), L = [E], m15_formal(E, F)), solutions([F = type_error(callable, (true ; 1))])).
test(m15_ite_whole, (m15_ite(E), m15_formal(E, F)), solutions([F = type_error(callable, (fail -> true ; (true, 1)))])).
test(m15_float, (m15_float(E), m15_formal(E, F)), solutions([F = type_error(callable, (true, 1.5))])).
test(m15_big, (m15_big(E), m15_formal(E, F)), solutions([F = type_error(callable, (true, 9223372036854775807))])).
test(m15_lone, (m15_lone(E), m15_formal(E, F)), solutions([F = type_error(callable, 1)])).
test(m15_nested_inner, (m15_nested(I, O), var(O), m15_formal(I, F)), solutions([F = type_error(callable, (true, 1))])).
test(m15_throw_not_reached, (m15_throw_first(E), m15_formal(E, F)), solutions([F = type_error(callable, (throw(a), 1))])).
test(m15_cut_not_reached, (m15_cut_first(L), L = [E], m15_formal(E, F)), solutions([F = type_error(callable, (m14_m(_), !, 1))])).
% 外の連言の 2 は catch の外なので、外の catch が何も動かす前に全体で捕まえる。
test(m15_outside_still_checked, (retractall(m15_seen), m15_outside(E), m15_formal(E, F), \+ m15_seen), solutions([F = type_error(callable, (assertz(m15_seen), catch(1, _, true), 2))])).
test(m15_call_inner, (catch(call(catch((true, 1), E, true)), O, true), var(O), m15_formal(E, F)), solutions([F = type_error(callable, (true, 1))])).
test(m15_call_lone, (call(catch(1, E, true)), m15_formal(E, F)), solutions([F = type_error(callable, 1)])).
test(m15_call_prefix_runs, (retractall(m15_seen), call((assertz(m15_seen), catch(1, _, true))), m15_seen), true).
test(m15_call_outside, (retractall(m15_seen), catch(call((assertz(m15_seen), catch(true, _, true), 1)), E, true), m15_formal(E, F), \+ m15_seen), solutions([F = type_error(callable, (assertz(m15_seen), catch(true, _, true), 1))])).
test(m15_findall, findall(F, (catch((true, 1), E, true), m15_formal(E, F)), L), solutions([L = [type_error(callable, (true, 1))]])).
test(m15_qualified, (user:catch((true, 1), E, true), m15_formal(E, F)), solutions([F = type_error(callable, (true, 1))])).
test(m15_with_output, (with_output_to(atom(A), catch((write(a), 1), _, true)), A == ''), true).
test(m15_assert, (retractall(m15_dyn(_)), assertz((m15_dyn(E) :- catch((true, 1), E, true))), m15_dyn(E1), m15_formal(E1, F)), solutions([F = type_error(callable, (true, 1))])).
test(m15_assert_shown, (retractall(m15_dyn(_)), assertz((m15_dyn(E) :- catch((true, 1), E, true))), clause(m15_dyn(_), B), B = catch(G, _, true), G == (true, 1)), true).
test(m15_clause_shown, (clause(m15_goal(_), B), B = catch(G, _, true), G == (assertz(m15_seen), 1)), true).
test(m15_clause_lone_shown, (clause(m15_lone(_), B), B = catch(G, _, true), G == 1), true).
% 節として訳す側（assertz）では Call が数をデータのまま持つ。Goal や Recovery が数だけでも節は入る。
test(m15_assert_lone, (retractall(m15_dyn(_)), assertz((m15_dyn(E) :- catch(1, E, true))), m15_dyn(E1), m15_formal(E1, F)), solutions([F = type_error(callable, 1)])).
test(m15_assert_lone_recovery, (retractall(m15_dyn(_)), assertz((m15_dyn(_) :- catch(throw(x), _, 2))), catch(m15_dyn(_), E, true), m15_formal(E, F)), solutions([F = type_error(callable, 2)])).
test(m15_assert_recovery_shown, (retractall(m15_dyn(_)), assertz((m15_dyn(_) :- catch(throw(x), _, (true, 2)))), clause(m15_dyn(_), B), B = catch(throw(x), _, R), R == (true, 2)), true).
test(m15_call_recovery, (catch(call(catch(throw(x), _, (assertz(m15_seen), 1))), E, true), m15_formal(E, F)), solutions([F = type_error(callable, (assertz(m15_seen), 1))])).
% 項として作った深い入れ子（kShallow の 64 段を超える）の中の catch/3 も同じ。
m15_deep(0, catch((true, 1), _, true)) :- !.
m15_deep(N, (true, G)) :- N1 is N - 1, m15_deep(N1, G).
m15_deep_catch(N, F) :- m15_deep(N, G), m15_last(G, catch(_, E, _)), call(G), m15_formal(E, F).
m15_last((_, G), C) :- !, m15_last(G, C).
m15_last(C, C).
test(m15_deep_call, m15_deep_catch(200, F), solutions([F = type_error(callable, (true, 1))])).
test(m15_deep_assert, (retractall(m15_dyn(_)), m15_deep(200, G), m15_last(G, catch(_, E, _)), assertz((m15_dyn(E) :- G)), m15_dyn(E1), m15_formal(E1, F)), solutions([F = type_error(callable, (true, 1))])).
% 変数があるときの M14 の振る舞いは変わらない。X は call/1 が変換したときに未束縛なので call(X) になる。
% 変数 1 つだけの Goal は今のまま Var の葉で、call/1 と同じ具体化エラーになる（型エラーではない）。
test(m15_call_anon, (call(catch(_, E, true)), m15_formal(E, F)), solutions([F = instantiation_error])).
test(m15_var_bound_later, (catch((X = 1, X), E, true), m15_formal(E, F)), solutions([F = type_error(callable, 1)])).
test(m14_clause_written_call, (clause(m14_written(_), B), B = catch(G, _, true), G = call(_)), true).
test(m14_written_call, m14_written(!), fail).
test(m14_assert_retract, (assertz((m14_dyn(X) :- catch(((X, fail) ; true), _, true))), \+ m14_dyn(!), retract((m14_dyn(Y) :- catch(((Y, fail) ; true), _, true))), \+ clause(m14_dyn(_), _)), true).
% レビュー M13。巡回するゴール（7.3.3 の STO の項）を呼んでも落ちない。巡回は無限に展開した項として動く。
% 葉の巡回: 引数に自分を含む述語の呼び出しは、その巡回する項を引数にした普通の呼び出しになる。
:- dynamic(m13_c/1).
m13_f(_).
m13_same(T) :- T = m13_same(U), U == T.
m13_tick(N) :- retract(m13_c(C)), N is C + 1, assertz(m13_c(N)).
m13_step :- retract(m13_c(C)), C1 is C + 1, assertz(m13_c(C1)), C1 < 100.
test(m13_leaf_call, (X = m13_f(X), call(X)), true).
test(m13_leaf_var_goal, (X = m13_f(X), X), true).
test(m13_leaf_arg_is_term, (X = m13_same(X), call(X)), true).
test(m13_leaf_undefined, (X = m13_nosuch(X), catch(call(X), error(existence_error(procedure, PI), _), true)), solutions([PI = m13_nosuch/1])).
test(m13_leaf_findall, (X = m13_f(X), findall(a, X, L)), solutions([L = [a]])).
test(m13_leaf_not, (X = m13_f(X), \+ \+ X), true).
test(m13_leaf_once, (X = m13_f(X), once(X)), true).
test(m13_leaf_catch, (X = m13_f(X), catch(X, _, fail)), true).
test(m13_leaf_qualified, (X = m13_f(X), user:X), true).
test(m13_leaf_forall, (X = m13_f(X), forall(X, true)), true).
test(m13_leaf_with_output, (X = m13_f(X), with_output_to(atom(A), X), A == ''), true).
% 制御構造の巡回: (a, X) は a を呼び、(G ; X) は G の解を順に返し続ける。
test(m13_conj_undefined, (X = (m13_undef, X), catch(call(X), error(existence_error(procedure, PI), _), true)), solutions([PI = m13_undef/0])).
% 巡回の各段は同じ変数を共有するので、段ごとの数は変数に束縛せずに数える。
test(m13_conj_runs, (retractall(m13_c(_)), assertz(m13_c(0)), X = (m13_step, X), \+ call(X), m13_c(M)), solutions([M = 100])).
test(m13_disj_first, (X = (Y = 1 ; X), once(X)), solutions([Y = 1])).
test(m13_disj_many, (retractall(m13_c(_)), assertz(m13_c(0)), X = (m13_tick(N) ; X), once((call(X), N >= 200))), solutions([N = 200])).
test(m13_ite_cycle, (X = (fail -> true ; (Y = 2 ; X)), once(X)), solutions([Y = 2])).
test(m13_catch_cycle, (X = catch((m13_undef2, X), error(existence_error(_, PI), _), true), call(X)), solutions([PI = m13_undef2/0])).
test(m13_not_cycle, (X = (\+ m13_undef3, X), catch(call(X), error(existence_error(_, PI), _), true)), solutions([PI = m13_undef3/0])).
% 共有する部分木は巡回ではない。同じ (a ; b) を 2 度通る。
test(m13_shared_not_cycle, (G = (Y = a ; Y = b), findall(Y-Z, (G, Z = Y ; G), L)), solutions([L = [a-a, b-b, a-_, b-_]])).
% 巡回の中のカットは、その段を訳した call/1 に閉じる。最初の段のカットが ; の残りを切るので失敗する。
test(m13_cut_in_cycle, (X = ((!, fail) ; X), \+ X), true).
% 巡回の入口が述語の引数なら、throw/1 と catch/3 は元の巡回する項をそのまま渡す。
test(m13_throw_cyclic, (X = m13_f(X), catch(throw(X), B, true), B == X), true).
% 巡回の戻り辺は call/1 として訳す（iso-notes 7.3.3）。2 段目のカットは 2 段目の中だけを切るので、1 段目の
% ; Y = extra は残る。展開して書いた有限のゴールなら、2 段目のカットが外まで切って [x] になる。
test(m13_cut_local_per_pass, (X = (((Y == x, !) ; (Y = x, X)) ; Y = extra), findall(Y, X, L)), solutions([L = [x, extra]])).
% 共有する部分木は巡回ではないので、中のカットは呼んだゴール全体を切る。戻り辺と間違えると [1, 2, 1, 3] になる。
test(m13_shared_cut, (S = (!, Y = 1), findall(Y, ((S ; Y = 2) ; (S ; Y = 3)), L)), solutions([L = [1]])).
m13_wrap(0, G, G) :- !.
m13_wrap(N, G, (true, W)) :- M is N - 1, m13_wrap(M, G, W).
test(m13_shared_cut_deep, (S = (!, Y = 1), m13_wrap(100, ((S ; Y = 2) ; (S ; Y = 3)), G), findall(Y, G, L)), solutions([L = [1]])).
test(m13_cut_local_deep, (m13_wrap(100, (((Y == x, !) ; (Y = x, X)) ; Y = extra), X), findall(Y, X, L)), solutions([L = [x, extra]])).
% レビュー M16。\+ G と once(G) は G を call/1 として動かす（8.15.1.1、8.15.2.1）。呼べない部分があれば、何も動かす
% 前に type_error(callable, G) になる（7.8.3.3）。forall/2 は \+ (Cond, \+ Action) なので、Cond は動かす前に
% 調べる。Action を調べるのは Cond が成功して Action を動かすときで、そのときも Action の何かが動く前に型エラーに
% なる（iso-notes forall/2）。
:- dynamic(m16_seen/0).
m16_not(E) :- catch(\+ (assertz(m16_seen), 1), E, true).
m16_once(E) :- catch(once((assertz(m16_seen), 1)), E, true).
m16_forall_cond(E) :- catch(forall((assertz(m16_seen), 1), true), E, true).
m16_forall_action(E) :- catch(forall(true, (assertz(m16_seen), 1)), E, true).
m16_formal(E, F) :- E = error(F, _).
m16_pick(1).
m16_pick(2).
m16_wrap(0, G, G) :- !.
m16_wrap(N, G, (true, W)) :- M is N - 1, m16_wrap(M, G, W).
test(m16_not_body, (retractall(m16_seen), m16_not(E), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_not_goal, (retractall(m16_seen), catch(\+ (assertz(m16_seen), 1), E, true), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_not_disj, (retractall(m16_seen), catch(\+ ((assertz(m16_seen), fail) ; 1), E, true), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, ((assertz(m16_seen), fail) ; 1))])).
test(m16_not_ite, (catch(\+ (fail -> true ; (true, 1.5)), E, true), m16_formal(E, F)), solutions([F = type_error(callable, (fail -> true ; (true, 1.5)))])).
test(m16_not_bound_later, (X = 1, catch(\+ (true, X), E, true), m16_formal(E, F)), solutions([F = type_error(callable, (true, 1))])).
test(m16_not_deep, (retractall(m16_seen), m16_wrap(200, 1, G), catch(\+ (assertz(m16_seen), G), E, true), m16_formal(E, F), F = type_error(callable, C), C == (assertz(m16_seen), G), \+ m16_seen), true).
test(m16_not_lone, (catch(\+ 1, E, true), m16_formal(E, F)), solutions([F = type_error(callable, 1)])).
test(m16_not_var, (catch(\+ (true, _), E, true), m16_formal(E, F)), solutions([F = instantiation_error])).
test(m16_not_call, (retractall(m16_seen), catch(call(\+ (assertz(m16_seen), 1)), E, true), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_not_findall, (retractall(m16_seen), findall(F, (catch(\+ (assertz(m16_seen), 1), E, true), m16_formal(E, F)), L), \+ m16_seen), solutions([L = [type_error(callable, (assertz(m16_seen), 1))]])).
test(m16_not_with_output, (with_output_to(atom(A), catch(\+ (write(a), 1), _, true)), A == ''), true).
test(m16_not_still_negates, (\+ fail, \+ \+ true, \+ (X = 1, fail), var(X), \+ (m16_pick(Y), Y > 2)), true).
test(m16_not_cut_local, (\+ (!, fail ; true)), true).
test(m16_once_goal, (retractall(m16_seen), m16_once(E), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_once_disj, (retractall(m16_seen), catch(once(((assertz(m16_seen), fail) ; 1)), E, true), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, ((assertz(m16_seen), fail) ; 1))])).
test(m16_once_deep, (retractall(m16_seen), m16_wrap(200, 2, G), catch(once((assertz(m16_seen), G)), E, true), m16_formal(E, F), F = type_error(callable, C), C == (assertz(m16_seen), G), \+ m16_seen), true).
test(m16_once_lone, (catch(once(1), E, true), m16_formal(E, F)), solutions([F = type_error(callable, 1)])).
test(m16_once_still_once, (once(m16_pick(X)), X == 1, \+ once((!, fail ; true))), true).
test(m16_forall_cond, (retractall(m16_seen), m16_forall_cond(E), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_forall_action, (retractall(m16_seen), m16_forall_action(E), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_forall_action_bound, (catch(forall(m16_pick(X), (X > 0, 1)), E, true), m16_formal(E, F)), solutions([F = type_error(callable, (1 > 0, 1))])).
test(m16_forall_action_unreached, forall(fail, 1), true).
test(m16_forall_still_works, (forall(m16_pick(X), X > 0), \+ forall(m16_pick(X), X > 1)), true).
test(m16_once_var, (catch(once((true, _)), E, true), m16_formal(E, F)), solutions([F = instantiation_error])).
test(m16_forall_var, (catch(forall((true, _), true), E, true), m16_formal(E, F)), solutions([F = instantiation_error])).
test(m16_call_not_extra, (retractall(m16_seen), catch(call(\+, (assertz(m16_seen), 1)), E, true), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_call_once_extra, (retractall(m16_seen), catch(call(once, (assertz(m16_seen), 1)), E, true), m16_formal(E, F), \+ m16_seen), solutions([F = type_error(callable, (assertz(m16_seen), 1))])).
test(m16_not_cyclic, (X = (X, 1), catch(\+ X, E, true), m16_formal(E, F), F = type_error(callable, C), C == X), true).

% レビュー M28。実行時に訳すゴールは束縛済みの複合項を写さず、元の項を指す。深さ 40 の共有項はヒープでは f/2 が
% 40 個だが、木に広げると 2^40 個になる。前は call/1 などがゴールを IR に写すときに広げたので止まらなかった。
% 共有項は答えの変数に出さない。差分テストが答えを書いて比べるので、書くと 2^40 個になる。
:- dynamic(m28_seen/0).
m28_dag(0, a) :- !.
m28_dag(N, f(T, T)) :- M is N - 1, m28_dag(M, T).
% 引数 1000 個のゴールを 70 個つなぎ、束縛済みの引数を 69930 個にする。引数はどれも別のセルの f(E)。スロットを
% 取り違えると、ある f(E) の位置に V の値 v が入って頭と合わない。歩みは少なく保つ。MOSA_GC_STRESS=1 は歩みごとに GC を走らせ、7 万の歩みでは
% 400 秒を超えた。
m28_fs(0, _, [], []) :- !.
m28_fs(N, E, [f(_)|T], [f(E)|U]) :- M is N - 1, m28_fs(M, E, T, U).
m28_wide(_, _, _, 0, Last, Last) :- !.
m28_wide(Fs, As, V, N, Last, (G, Rest)) :-
    copy_term(Fs, Args), Args = As, G =.. [m28_w, V|Args], M is N - 1, m28_wide(Fs, As, V, M, Last, Rest).
% 連言の終わりは Last（true か !）。
m28_wide_goal(E, Last, V, G) :-
    m28_fs(999, E, Fs, As), copy_term(Fs, HeadArgs), H =.. [m28_w, W|HeadArgs], retractall(H),
    assertz((H :- W == v)), m28_wide(Fs, As, V, 70, Last, G).
% 同じ 70 個のゴールを釣り合った連言（深さ 8）にする。浅い道（goal_at）でも 2^16 個を超えるスロットを使う。
m28_chain_list(true, Last, [Last]) :- !.
m28_chain_list((G, Rest), Last, [G|L]) :- m28_chain_list(Rest, Last, L).
m28_pairs([A, B|T], [(A, B)|U]) :- !, m28_pairs(T, U).
m28_pairs(L, L).
m28_tree([G], G) :- !.
m28_tree(L, G) :- m28_pairs(L, P), m28_tree(P, G).
m28_tree_goal(Last, V, G) :- m28_wide_goal(a, true, V, Chain), m28_chain_list(Chain, Last, L), m28_tree(L, G).
m28_run(call) :- m28_dag(40, X), call(X \== b).
m28_run(call_built) :- m28_dag(40, X), G = (X \== b), call(G).
m28_run(var_goal) :- m28_dag(40, X), G = (X \== b), G.
m28_run(not_not) :- m28_dag(40, X), \+ \+ X \== b.
m28_run(not) :- m28_dag(40, X), \+ X \== b.
m28_run(call_extra) :- m28_dag(40, X), call(\==, X, b).
% 節の本体に書いた catch/3 は読み込みのときに訳すので、実行時に組んだ catch/3 を呼ぶ。
m28_run(catch) :- m28_dag(40, X), call(catch(X \== b, _, true)).
m28_run(catcher) :- m28_dag(40, X), call(catch(throw(X), B, true)), B \== b.
m28_run(findall) :- m28_dag(40, X), findall(t, X \== b, L), L == [t].
m28_run(once) :- m28_dag(40, X), once((X \== b ; true)).
m28_run(if_then_else) :- m28_dag(40, X), call((X == b -> R = yes ; R = no)), R == no.
% 束縛済みの引数ごとにスロットが要る。M28 は 2^16 個を超えそうなら残りのゴールを後で訳し、それでも溢れたら写し直した
% （Codex 敵対的レビュー。前はすべてを写し直して、共有項の引数 f(X) が 69930 個あると、ここでまた木に広げた）。
% M31（ADR 0021）でスロットは 32 ビットになり、写し直す段は無くなった。後で訳す段は、動かない残りを訳さずにおくので
% 残る。下の検査は、後で訳す規則（カット、ゴール位置の変数、呼べない項、if-then-else）を確かめる。
m28_run(many_compound_args) :- m28_wide_goal(a, true, V, G), call((V = v, G)).
m28_run(many_shared_args) :- m28_dag(40, X), m28_wide_goal(X, true, V, G), call((V = v, G)).
% 残りの中の ! は前の選択点を切るので、残りを後で訳せない（call/1 の中では局所になる）。小さい引数を写して訳し直し、
% 共有の多い大きい項は指したまま。
m28_run(many_compound_args_cut) :- m28_wide_goal(a, !, V, G), findall(Y, call((V = v, (Y = 1 ; Y = 2), G)), L), L == [1].
m28_run(many_compound_args_dag) :- m28_wide_goal(a, !, V, G), m28_dag(40, X), call((V = v, X \== b, G)).
% 呼べない部分のある残りも後で訳さない。call/1 は何も動かす前に type_error(callable, _) を出す（7.8.3.3）。
m28_run(many_compound_args_bad) :-
    m28_wide_goal(a, 1, V, G), retractall(m28_seen),
    catch(call((assertz(m28_seen), V = v, G)), error(type_error(callable, _), _), true), \+ m28_seen.
% M:G はカットの壁を引き継ぐので、user:! も残りを後で訳させない（Codex 敵対的レビュー 3 回目）。
m28_run(many_compound_args_qualified_cut) :-
    m28_wide_goal(a, user:!, V, G), findall(Y, call((V = v, (Y = 1 ; Y = 2), G)), L), L == [1].
% ゴール位置の未束縛の変数は call(C) として動く。後で訳すと、その時の束縛 ! がじかのカットになり、1 が
% 残り全体の type_error になる（Codex レビュー 3 回目、Codex 敵対的レビュー 3 回目）。
m28_run(many_compound_args_var_cut) :-
    m28_wide_goal(a, ((Y = 1 ; Y = 2), C), V, G), findall(Y, call((C = !, V = v, G)), L), L == [1, 2].
m28_run(many_compound_args_var_bad) :-
    m28_wide_goal(a, (assertz(m28_seen), C), V, G), retractall(m28_seen),
    catch(call((C = 1, V = v, G)), error(type_error(callable, Culprit), _), true), Culprit == 1, m28_seen.
% 後で訳せない ! が終わりにあっても、その前の葉は 1 つずつ後で訳すので、共有の多い引数を写さない。
m28_run(many_shared_args_cut) :- m28_dag(40, X), m28_wide_goal(X, !, V, G), call((V = v, G)).
% ; の左の -> と *-> を Call に包むとただの選言になり、then の fail の後に else が動く。else の ! で ; は後で訳せず、
% 左だけを後で訳しそうになる（Codex レビュー 4 回目、Codex 敵対的レビュー 4 回目）。深い道と浅い道の両方。
m28_run(if_then_late) :-
    m28_wide_goal(a, (a = a -> fail ; !, assertz(m28_seen)), V, G), retractall(m28_seen), \+ call((V = v, G)), \+ m28_seen.
m28_run(soft_if_late) :-
    m28_wide_goal(a, ((a = a) *-> fail ; !, assertz(m28_seen)), V, G), retractall(m28_seen), \+ call((V = v, G)), \+ m28_seen.
m28_run(if_then_late_shallow) :-
    m28_tree_goal((a = a -> fail ; !, assertz(m28_seen)), V, G), retractall(m28_seen), \+ call((V = v, G)), \+ m28_seen.
m28_run(soft_if_late_shallow) :-
    m28_tree_goal(((a = a) *-> fail ; !, assertz(m28_seen)), V, G), retractall(m28_seen), \+ call((V = v, G)), \+ m28_seen.
test(m28_call_dag, m28_run(call), true).
test(m28_call_built_dag, m28_run(call_built), true).
test(m28_var_goal_dag, m28_run(var_goal), true).
test(m28_not_not_dag, m28_run(not_not), true).
test(m28_not_dag, m28_run(not), fail).
test(m28_call_extra_dag, m28_run(call_extra), true).
test(m28_catch_dag, m28_run(catch), true).
test(m28_catcher_dag, m28_run(catcher), true).
test(m28_findall_dag, m28_run(findall), true).
test(m28_once_dag, m28_run(once), true).
test(m28_ite_dag, m28_run(if_then_else), true).
test(m28_many_compound_args, m28_run(many_compound_args), true).
test(m28_many_shared_args, m28_run(many_shared_args), true).
test(m28_many_compound_args_cut, m28_run(many_compound_args_cut), true).
test(m28_many_compound_args_dag, m28_run(many_compound_args_dag), true).
test(m28_many_compound_args_bad, m28_run(many_compound_args_bad), true).
test(m28_many_compound_args_qualified_cut, m28_run(many_compound_args_qualified_cut), true).
test(m28_many_compound_args_var_cut, m28_run(many_compound_args_var_cut), true).
test(m28_many_compound_args_var_bad, m28_run(many_compound_args_var_bad), true).
test(m28_many_shared_args_cut, m28_run(many_shared_args_cut), true).
test(m28_if_then_late, m28_run(if_then_late), true).
test(m28_soft_if_late, m28_run(soft_if_late), true).
test(m28_if_then_late_shallow, m28_run(if_then_late_shallow), true).
test(m28_soft_if_late_shallow, m28_run(soft_if_late_shallow), true).
% 元の項を指しても、中の変数は同じ変数のまま。ゴールの中で束縛すれば、指した項からも見える。
test(m28_inner_var, (X = f(Y), call((Y = 1, X == f(1)))), true).
test(m28_inner_var_later, (X = g(f(Y)), call((X = g(Z), Y = 2, Z == f(2)))), true).
test(m28_inner_var_undo, (X = f(Y), call(((Y = 1, fail) ; var(Y))), X = f(Z), var(Z)), true).
test(m28_cyclic_arg, (X = f(X), call(X = f(Y)), Y == X), true).

% レビュー M31（ADR 0021）。スロットの番号は 32 ビット。前は 16 ビットで、変数 70000 個の節は番号が 2^16 で折り返して
% env の外を指し、プロセスごと落ちた。どちらも組込の数歩で済むので、MOSA_GC_STRESS=1 でも重くない。bagof/3 の
% 証人の検査は、答えを結ぶ = を証人ごとに 1 歩ずつ動かすので、ストレスを 1000 にする ctest goal_shared に置く。
:- dynamic(m31_p/1).
:- dynamic(m31_q/1).
test(m31_clause_head_vars, (length(L, 70000), retractall(m31_p(_)), assertz(m31_p(L)), m31_p(M), sort(M, S), length(S, 70000)), true).
test(m31_clause_body_vars, (length(L, 70000), retractall(m31_q(_)), assertz((m31_q(X) :- X = L)), m31_q(M), sort(M, S), length(S, 70000)), true).
