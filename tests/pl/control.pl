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
% 外した Entry は op/3 の記録の持ち主にも、consult の「実行中」にもならない。once/1 と -> の後と同じ扱いになる。
op_kept(Op, K) :- ( current_op(_, _, Op) -> K = kept ; K = undone ).
test(op_exit_same, ((consult('tests/pl/modules/op_exit.pl'), fail ; true), op_kept('=once=>', A), op_kept('=catch=>', B), op_kept('=soft=>', C), op_kept('=ite=>', D), A == B, B == C, C == D), true).
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
test(m14_clause_written_call, (clause(m14_written(_), B), B = catch(G, _, true), G = call(_)), true).
test(m14_written_call, m14_written(!), fail).
test(m14_assert_retract, (assertz((m14_dyn(X) :- catch(((X, fail) ; true), _, true))), \+ m14_dyn(!), retract((m14_dyn(Y) :- catch(((Y, fail) ; true), _, true))), \+ clause(m14_dyn(_), _)), true).
