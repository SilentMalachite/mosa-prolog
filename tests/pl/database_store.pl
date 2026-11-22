% database.pl の続き（800 行の上限で分けた）。assert の本体の形（変数の本体と巡回する本体、レビュー H5 と M7）、
% 鍵の走査のカーソル（M20）、abolish の後の述語の索引（M25）、共有の深い項（M30）を見る。述語はこのファイルの中で
% 定義し、database.pl の述語も状態も使わない。

% 8.9.1.3・8.9.2.3 は頭部の変数だけを誤りとし、7.6.2 は変数の本体を call/1 に変える（iso-notes 8.9.1、レビュー H5）。
:- dynamic(bv_order/0).
bv_order :- fail.
bv_static :- true.
test(assert_body_var_asserta, asserta((bv_a :- _)), true).
test(assert_body_var_asserta_clause, (clause(bv_a, B), nonvar(B), B = call(V), var(V)), true).
test(assert_body_var_assertz_call, (assertz((bv_z(X) :- X)), bv_z(true)), true).
test(assert_body_var_assertz_fail, bv_z(fail), fail).
test(assert_body_var_unbound_call, bv_z(_), error(instantiation_error)).
test(assert_body_var_number_call, bv_z(1), error(type_error(callable, 1))).
% 頭部と本体が同じセルのまま格納される。頭の引数を本体として呼ぶ。
test(assert_body_var_shared, (clause(bv_z(Y), B), B == call(Y)), true).
test(assert_body_var_solutions, (assertz((bv_m(X) :- X)), bv_m((Z = 1 ; Z = 2 ; Z = 3))), solutions([Z = 1, Z = 2, Z = 3])).
% call/1 は切断を閉じる。本体へ ! をそのまま埋めると、二つ目の節を切ってしまう。
test(assert_body_var_cut_opaque, (assertz((bv_c(X) :- X)), assertz(bv_c(_)), findall(k, bv_c(!), L)), solutions([L = [k, k]])).
test(assert_body_var_cut_local, (findall(Z, bv_m(((Z = 1 ; Z = 2 ; Z = 3), !)), L)), solutions([L = [1]])).
% assert の後で束縛しても、格納された節の変数は動かない。
test(assert_body_var_copied, (assertz((bv_cp :- G)), G = true, catch(bv_cp, error(instantiation_error, _), R = caught), R == caught), true).
test(assert_body_var_asserta_first, (asserta((bv_order :- true)), asserta((bv_order :- _)), clause(bv_order, B)), solutions([B = call(_), B = true, B = fail])).
test(assert_body_var_qualified, (assertz(user:(bv_q(X) :- X)), bv_q(true)), true).
test(assert_body_var_retract, (assertz((bv_r :- _)), retract((bv_r :- B)), nonvar(B), B = call(V), var(V)), true).
test(assert_body_var_retract_gone, (assertz((bv_rg :- _)), retract((bv_rg :- _)), \+ clause(bv_rg, _), assertz((bv_rg :- _)), clause(bv_rg, B)), solutions([B = call(_)])).
test(assert_body_var_retract_call, (assertz((bv_rc :- _)), retract((bv_rc :- call(_)))), true).
% retract は clause/2 と同じ本体を見る。格納した変数のまま単一化すると、どの本体とも合ってしまう。
test(assert_body_var_retract_fact_first, (assertz((bv_rf :- _)), assertz(bv_rf), retract(bv_rf), clause(bv_rf, B)), solutions([B = call(_)])).
test(assert_body_var_retract_true, (assertz((bv_rt :- _)), assertz((bv_rt :- true)), retract((bv_rt :- true)), findall(B, clause(bv_rt, B), L)), solutions([L = [call(_)]])).
test(assert_body_var_retract_head_only, (assertz((bv_rh :- _)), retract(bv_rh)), fail).
test(assert_body_var_retract_other_goal, (assertz((bv_ro :- _)), retract((bv_ro :- foo(1)))), fail).
test(assert_body_var_retract_head_arg, (assertz((bv_ra(X) :- X)), retract((bv_ra(_) :- foo))), fail).
test(assert_body_var_retract_shared, (assertz((bv_rs(X) :- X)), findall(Y-B, retract((bv_rs(Y) :- B)), [Y1-B1]), B1 == call(Y1)), true).
test(assert_body_var_retract_conj, (assertz((bv_rj(X) :- true, X)), retract((bv_rj(_) :- (true, foo)))), fail).
test(assert_body_var_retract_conj_call, (assertz((bv_rk(X) :- true, X)), retract((bv_rk(Y) :- (true, B))), B == call(Y)), true).
test(assert_body_var_retract_halt, (assertz((bv_hl :- halt)), retract((bv_hl :- halt))), true).
% catch/3 の引数の変数は call/1 に包まない。本体の false は fail に畳む（iso-notes 8.8 clause/2）ので、それを除けば assert した形で消せる。
test(assert_body_var_retract_catch, (assertz((bv_ct(G) :- catch(G, _, fail))), retract((bv_ct(foo) :- catch(foo, _, fail)))), true).
test(assert_body_var_retract_catch_var, (assertz((bv_cv(G) :- catch(G, _, true))), retract((bv_cv(X) :- catch(X, _, true))), var(X)), true).
test(assert_body_var_retract_left_nested, (assertz((bv_ln(X) :- ((X, a), b))), retract((bv_ln(Y) :- ((B, a), b))), B == call(Y)), true).
test(assert_body_var_retract_halt_inside, (assertz((bv_hi :- ((a ; halt) -> b ; c))), retract((bv_hi :- ((a ; halt) -> b ; c)))), true).
% 事実を消す retract(Head) は本体 true の節だけを見る。
test(assert_body_var_retract_rule_true, (assertz((bv_tr :- true)), assertz((bv_tr :- _)), retract(bv_tr), clause(bv_tr, B)), solutions([B = call(_)])).
% 頭部の誤りが本体より先。静的と組込の拒否も本体が変数で変わらない。
test(assert_body_var_both_var, assertz((_ :- _)), error(instantiation_error)).
test(assert_body_var_head_number, asserta((1 :- _)), error(type_error(callable, 1))).
test(assert_body_var_builtin, assertz((atom(_) :- _)), error(permission_error(modify, static_procedure, atom/1))).
test(assert_body_var_control, asserta((call(_) :- _)), error(permission_error(modify, static_procedure, call/1))).
test(assert_body_var_static, assertz((bv_static :- _)), error(permission_error(modify, static_procedure, bv_static/0))).
test(assert_body_var_conj_number, assertz((bv_n :- (_, 1))), error(type_error(callable, (_, 1)))).
% レビュー M7: 本体が制御構造で巡回する節は、戻り先を名前のない変数のゴールに切って足す（オーナー決定 A、
% iso-notes 7.3.3）。頭や引数のデータの巡回を匿名に切るのと同じ。そこまで来ると instantiation_error になる。
:- dynamic(m7_q/1).
m7_q(_).
m7_chain(0, T, T) :- !.
m7_chain(N, T, (true, R)) :- M is N - 1, m7_chain(M, T, R).
test(m7_assert_conj, (X = (m7_undef, X), assertz((m7_p1 :- X)), catch(m7_p1, error(existence_error(procedure, PI), _), true)), solutions([PI = m7_undef/0])).
test(m7_assert_conj_back, (X = (true, X), assertz((m7_p2 :- X)), catch(m7_p2, error(E, _), true)), solutions([E = instantiation_error])).
test(m7_assert_clause_view, (X = (m7_a, X), assertz((m7_p3 :- X)), clause(m7_p3, B), B = (m7_a, call(V)), var(V)), true).
test(m7_assert_disj, (X = (Y = 1 ; X), assertz((m7_p4(Y) :- X)), findall(Y, catch(m7_p4(Y), error(instantiation_error, _), Y = err), L)), solutions([L = [1, err]])).
test(m7_assert_ite, (X = (fail -> true ; X), assertz((m7_p5 :- X)), catch(m7_p5, error(E, _), true)), solutions([E = instantiation_error])).
test(m7_assert_soft, (X = (fail *-> true ; X), assertz((m7_p6 :- X)), catch(m7_p6, error(E, _), true)), solutions([E = instantiation_error])).
test(m7_assert_catch, (X = catch(X, _, true), assertz((m7_p7 :- X)), m7_p7), true).
test(m7_assert_left, (X = (X, a), asserta((m7_p8 :- X)), catch(m7_p8, error(E, _), true)), solutions([E = instantiation_error])).
test(m7_assert_inner, (X = (m7_b, X), assertz((m7_p9 :- m7_c, X)), clause(m7_p9, B), B = (m7_c, m7_b, call(V)), var(V)), true).
test(m7_assert_deep, (m7_chain(100, G, G0), G = G0, assertz((m7_p10 :- G)), catch(m7_p10, error(E, _), true)), solutions([E = instantiation_error])).
% 戻り先の変数は制御構造ごとに 1 つ。ゴールの位置にしかないので、共有しても誰も束縛しない。
test(m7_assert_two_backs, (X = (Y ; Z), Y = (true, X), Z = (fail, X), assertz((m7_p11 :- X)), clause(m7_p11, (A, call(V1) ; B, call(V2))), A == true, B == fail, var(V1), V1 == V2), true).
test(m7_assert_two_targets, (X = (m7_q(1), Y), Y = (m7_q(2), X ; Y), assertz((m7_p13 :- X)), clause(m7_p13, (m7_q(1), (m7_q(2), call(V1) ; call(V2)))), var(V1), var(V2), V1 \== V2), true).
% 共有の多い巡回する本体は、戻り辺が 2^16 個あっても戻り先は 1 つなので変数は 1 つで済む。
m7_body_dag(0, L, L) :- !.
m7_body_dag(N, L, T) :- M is N - 1, m7_body_dag(M, L, T0), T = (T0 ; T0).
m7_body_dag_runs :- m7_body_dag(16, (true, X), X), assertz((m7_p14 :- X)), catch(m7_p14, error(instantiation_error, _), true).
test(m7_assert_body_dag, m7_body_dag_runs, true).
test(m7_assert_data_arg, (X = (m7_q(X), true), assertz((m7_p12 :- X)), m7_p12, clause(m7_p12, (m7_q(A), true)), A = (m7_q(V), true), var(V)), true).

% レビュー M20（ADR 0015 の改訂）。第1引数で引く走査は組の列を作らずに番号のカーソルで歩き、死んだ節は飛び越しの
% 幅で跳ぶ。答えと順は列を作っていたときと同じで、走査は始めた時点の節を見る（7.5.4）。索引は 8 節以上の述語だけが
% 持つので、どの述語も 9 節以上にする。
:- dynamic(m20_k/2).
m20_k(a, 1). m20_k(b, 2). m20_k(a, 3). m20_k(_, 4). m20_k(a, 5). m20_k(b, 6). m20_k(a, 7). m20_k(b, 8). m20_k(a, 9).
test(m20_keyed_call_view, findall(X, (m20_k(a, X), (X == 1 -> assertz(m20_k(a, 10)), asserta(m20_k(a, 0)), retract(m20_k(a, 7)), retract(m20_k(_, 4)) ; true)), L), solutions([L = [1, 3, 4, 5, 7, 9]])).
test(m20_keyed_call_view_left, findall(X, m20_k(a, X), L), solutions([L = [0, 1, 3, 5, 9, 10]])).
:- dynamic(m20_r/2).
m20_r(a, 1). m20_r(b, 2). m20_r(a, 3). m20_r(b, 4). m20_r(a, 5). m20_r(b, 6). m20_r(a, 7). m20_r(b, 8). m20_r(a, 9).
% retract の走査は別の retract が消した節を飛ばし、走査の後に足した節を見ない。
test(m20_keyed_retract_view, findall(X, (retract(m20_r(a, X)), (X == 1 -> assertz(m20_r(a, 10)), retract(m20_r(a, 5)) ; true)), L), solutions([L = [1, 3, 7, 9]])).
test(m20_keyed_retract_view_left, findall(K-X, m20_r(K, X), L), solutions([L = [b-2, b-4, b-6, b-8, a-10]])).
:- dynamic(m20_c/2).
m20_c(a, 1). m20_c(b, 2). m20_c(a, 3). m20_c(b, 4). m20_c(a, 5). m20_c(b, 6). m20_c(a, 7). m20_c(b, 8). m20_c(a, 9).
test(m20_keyed_clause_view, findall(X, (clause(m20_c(a, X), true), (X == 1 -> retract(m20_c(a, 5)), assertz(m20_c(a, 10)) ; true)), L), solutions([L = [1, 3, 5, 7, 9]])).
% 外の走査が届くので、消した先頭の節は列に残る。新しい呼び出しと retract はそれを見ず、外の走査は全部を見る。
:- dynamic(m20_d/1).
m20_d(1). m20_d(2). m20_d(3). m20_d(4). m20_d(5). m20_d(6). m20_d(7). m20_d(8). m20_d(9). m20_d(10).
m20_d_drain(0) :- !.
m20_d_drain(N) :- retract(m20_d(_)), !, M is N - 1, m20_d_drain(M).
test(m20_dead_prefix, findall(X-L, (m20_d(X), (X == 1 -> m20_d_drain(4), findall(Y, m20_d(Y), L) ; L = [])), R), solutions([R = [1-[5, 6, 7, 8, 9, 10], 2-[], 3-[], 4-[], 5-[], 6-[], 7-[], 8-[], 9-[], 10-[]]])).
test(m20_dead_prefix_left, findall(X, m20_d(X), L), solutions([L = [5, 6, 7, 8, 9, 10]])).
% 2 を消した後に始めた内の走査は、その後に消した 3 と 4 をまだ見る。新しい呼び出しが 2 から 4 を跳んで道を縮めても、
% 内の走査はそれを跳ばない。跳べば 4 が欠ける。
:- dynamic(m20_g/1).
m20_g(1). m20_g(2). m20_g(3). m20_g(4). m20_g(5). m20_g(6). m20_g(7). m20_g(8). m20_g(9).
test(m20_skip_generations, (m20_g(X), X == 1, retract(m20_g(2)), findall(Y-Z, (m20_g(Y), (Y == 1 -> retract(m20_g(3)), retract(m20_g(4)), findall(W, m20_g(W), Z) ; Z = [])), L), !), solutions([L = [1-[1, 5, 6, 7, 8, 9], 3-[], 4-[], 5-[], 6-[], 7-[], 8-[], 9-[]]])).
% 同じことを鍵の組の中で。鍵 a の節の間に鍵 b の生きた節が挟まる。
:- dynamic(m20_gk/2).
m20_gk(a, 1). m20_gk(b, 0). m20_gk(a, 2). m20_gk(b, 0). m20_gk(a, 3). m20_gk(b, 0). m20_gk(a, 4). m20_gk(b, 0). m20_gk(a, 5). m20_gk(a, 6).
test(m20_skip_generations_keyed, (m20_gk(a, X), X == 1, retract(m20_gk(a, 2)), findall(Y-Z, (m20_gk(a, Y), (Y == 1 -> retract(m20_gk(a, 3)), retract(m20_gk(a, 4)), findall(W, m20_gk(a, W), Z) ; Z = [])), L), !), solutions([L = [1-[1, 5, 6], 3-[], 4-[], 5-[], 6-[]]])).
% 外の鍵の走査の下で鍵 a の節を前から消す。消した節は鍵 b の生きた節の間に残る。
:- dynamic(m20_ik/2).
:- dynamic(m20_ik_log/1).
m20_ik(a, 1). m20_ik(b, 1). m20_ik(a, 2). m20_ik(b, 2). m20_ik(a, 3). m20_ik(b, 3). m20_ik(a, 4). m20_ik(b, 4). m20_ik(a, 5). m20_ik(b, 5).
m20_ik_drain :- retract(m20_ik(a, X)), !, assertz(m20_ik_log(X)), m20_ik_drain.
m20_ik_drain.
test(m20_dead_interleaved, (findall(X, (m20_ik(a, X), (X == 1 -> m20_ik_drain ; true)), L), findall(Y, m20_ik_log(Y), Log), findall(K-V, m20_ik(K, V), Left)), solutions([(L = [1, 2, 3, 4, 5], Log = [1, 2, 3, 4, 5], Left = [b-1, b-2, b-3, b-4, b-5])])).
% 末尾まで死んだ節を跳ぶと、跳んだ先は次に振る番号になる。後で足した節はそこから見える。
:- dynamic(m20_e/2).
m20_e(k, 1). m20_e(k, 2). m20_e(k, 3). m20_e(k, 4). m20_e(k, 5). m20_e(k, 6). m20_e(k, 7). m20_e(k, 8). m20_e(k, 9).
m20_e_drain :- retract(m20_e(_, _)), !, m20_e_drain.
m20_e_drain.
test(m20_dead_to_end, findall(X-L1-L2-L3, (m20_e(k, X), (X == 1 -> m20_e_drain, findall(Y, m20_e(_, Y), L1), findall(Y, m20_e(k, Y), L2), assertz(m20_e(k, 20)), asserta(m20_e(k, 0)), findall(Y, m20_e(k, Y), L3) ; L1 = [], L2 = [], L3 = [])), R), solutions([R = [1-[]-[]-[0, 20], 2-[]-[]-[], 3-[]-[]-[], 4-[]-[]-[], 5-[]-[]-[], 6-[]-[]-[], 7-[]-[]-[], 8-[]-[]-[], 9-[]-[]-[]]])).
% retractall も鍵で歩き、外の走査が届く死んだ節を跳ぶ。
:- dynamic(m20_ra/2).
m20_ra(a, 1). m20_ra(b, 2). m20_ra(a, 3). m20_ra(b, 4). m20_ra(a, 5). m20_ra(b, 6). m20_ra(a, 7). m20_ra(b, 8). m20_ra(a, 9).
test(m20_retractall_keyed, findall(X-L, (m20_ra(a, X), (X == 1 -> retract(m20_ra(a, 3)), retractall(m20_ra(a, _)), findall(K-Y, m20_ra(K, Y), L) ; L = [])), R), solutions([R = [1-[b-2, b-4, b-6, b-8], 3-[], 5-[], 7-[], 9-[]]])).

% レビュー M25（ADR 0018）。表の前の方の述語を abolish すると、後ろの述語の位置が 1 つずつずれる。索引がずれを
% 直さなければ、後ろの述語は引けないか、表の外を読む。名前が同じでアリティだけが違う述語も別に引く。
:- dynamic(m25_a/1).
:- dynamic(m25_b/1).
:- dynamic(m25_b/2).
:- dynamic(m25_c/1).
m25_a(1).
m25_b(2).
m25_b(3, x).
m25_c(4).
test(m25_abolish_front, (abolish(m25_a/1), findall(X, m25_b(X), B), findall(X-Y, m25_b(X, Y), B2), findall(X, m25_c(X), C)), solutions([(B = [2], B2 = [3-x], C = [4])])).
test(m25_abolish_front_gone, m25_a(_), error(existence_error(procedure, m25_a/1))).
test(m25_abolish_then_assert, (assertz(m25_a(5)), retract(m25_c(4)), assertz(m25_c(6)), findall(X, m25_a(X), A), findall(X, m25_c(X), C), findall(X, m25_b(X), B)), solutions([(A = [5], C = [6], B = [2])])).
test(m25_abolish_middle, (abolish(m25_b/1), findall(X-Y, m25_b(X, Y), B2), findall(X, m25_c(X), C), findall(X, m25_a(X), A)), solutions([(B2 = [3-x], C = [6], A = [5])])).

% レビュー M30（ADR 0022）。共有の深い項を assert すると、前は節の IR を木に広げたので、深さ 40 の項は 2^40 個の節点に
% なって止まらなかった（ヒープの上限も超えた）。今は 2 度目に出会った部分項を、節に入るときに 1 度だけ組む。
% m30_shape は左の枝をたどり、各段の 2 つの子が同じ項であることを確かめる。共有を保てば == は段ごとにすぐ終わる。
% 共有の深い項はテストのゴールの変数に残さない。差分テスト（mosatest --diff）は答えを書いて比べ、書くのは木の大きさになる。
m30_dag(0, a) :- !.
m30_dag(N, f(T, T)) :- M is N - 1, m30_dag(M, T).
m30_shape(a, 0).
m30_shape(f(A, B), N) :- N > 0, A == B, M is N - 1, m30_shape(A, M).
:- dynamic(m30_p/1).
:- dynamic(m30_pa/1).
:- dynamic(m30_c/1).
:- dynamic(m30_r/1).
:- dynamic(m30_b/1).
:- dynamic(m30_hb/1).
:- dynamic(m30_t/0).
:- dynamic(m30_v/2).
:- dynamic(m30_cy/1).
:- dynamic(m30_o/1).
:- dynamic(m30_l/2).
:- dynamic(m30_d/3).
:- dynamic(m30_i/2).
:- dynamic(m30_bt/1).
:- dynamic(m30_re/1).
m30_run(assert_dag) :- m30_dag(40, X), assertz(m30_p(X)), m30_p(Y), m30_shape(Y, 40).
m30_run(asserta_dag) :- m30_dag(40, X), asserta(m30_pa(X)), m30_pa(Y), m30_shape(Y, 40).
m30_run(clause_dag) :- m30_dag(40, X), assertz(m30_c(X)), clause(m30_c(Y), B), B == true, m30_shape(Y, 40).
m30_run(retract_dag) :- m30_dag(40, X), assertz(m30_r(X)), retract(m30_r(Y)), m30_shape(Y, 40), \+ m30_r(_).
m30_run(body_dag) :- m30_dag(40, X), assertz((m30_b(Y) :- Y = g(X))), m30_b(g(Y)), m30_shape(Y, 40).
m30_run(head_body_dag) :- m30_dag(40, X), assertz((m30_hb(X) :- m30_shape(X, 40))), m30_hb(Y), m30_shape(Y, 40).
m30_run(throw_dag) :- m30_dag(40, X), assertz((m30_t :- throw(X))), catch(m30_t, B, true), m30_shape(B, 40).
m30_run(list_tail) :-
    m30_upto(1, 2000, [_|T]), assertz(m30_l(f(T), g(T))), m30_l(f(A), g(B)), A == B, length(A, 1999), A = [2|_],
    m30_last(A, 2000).
test(m30_assert_dag, m30_run(assert_dag), true).
test(m30_asserta_dag, m30_run(asserta_dag), true).
test(m30_clause_dag, m30_run(clause_dag), true).
test(m30_retract_dag, m30_run(retract_dag), true).
test(m30_body_dag, m30_run(body_dag), true).
test(m30_head_body_dag, m30_run(head_body_dag), true).
test(m30_throw_dag, m30_run(throw_dag), true).
% 1 度だけ組む部分項の中の変数も、節の他の所の同じ変数のままである。
test(m30_shared_var, (S = g(V), assertz(m30_v(k(h(S), h(S)), V)), m30_v(k(h(A), h(B)), W), A == B, A = g(W1), W1 == W), true).
% 循環に届く部分項は共有しない。循環を切る匿名は出現ごとに別の変数のまま（iso-notes 7.3.3 の決定 A）。
test(m30_cycle_anon, (X = f(X), assertz(m30_cy(k(h(X), h(X)))), m30_cy(k(h(f(A)), h(f(B)))), A \== B), true).
% clause/2 は前と同じく、変数を頭から左の前順に作る（変数の標準順序は番地で決まる）。
test(m30_clause_order, (S = g(_), assertz(m30_o(w(_, k(h(S), h(S)), _))), clause(m30_o(W), true), W = w(X, k(h(g(Y)), _), Z), X @< Y, Y @< Z), true).
% 長いリストの尾を 2 か所で共有しても、どちらの引数も元のリストになる。
m30_upto(N, N, [N]) :- !.
m30_upto(I, N, [I|T]) :- J is I + 1, m30_upto(J, N, T).
m30_last([X], X) :- !.
m30_last([_|T], X) :- m30_last(T, X).
test(m30_list_tail, m30_run(list_tail), true).
% 頭の第 1 引数のほかは、引数そのものも共有する。頭の第 1 引数は鍵のために写すので、索引も前のまま引く。
test(m30_arg_shared, (S = k(V), assertz(m30_d(a, S, S)), m30_d(a, X, Y), X == Y, X = k(W), var(W), clause(m30_d(a, P, Q), true), P == Q, retract(m30_d(a, R1, R2)), R1 == R2), true).
test(m30_first_arg_key, (S = k(1), assertz(m30_i(S, S)), assertz(m30_i(z, z)), findall(Q, m30_i(k(1), Q), L1), findall(Q, m30_i(z, Q), L2)), solutions([(L1 = [k(1)], L2 = [z])])).
% 共有の項は最初に要るときに組み、置き場の変数に束縛する。束縛より前の選択点へ戻れば、次に要るときに組み直す。
test(m30_rebuilt, (S = g(h(1)), assertz((m30_bt(R) :- (Y = 1 ; Y = 2), R = k(S, S, Y))), findall(R, m30_bt(R), L), L = [k(A1, B1, 1), k(A2, B2, 2)], A1 == B1, A2 == B2, A1 == g(h(1)), A2 == g(h(1))), true).
% NaN は自分とも単一化しない（iso-notes 7.2.2）。共有にすると同じ番地の近道で一致するので、M30 の前と同じく写しを分ける。
% 小さい部分項はもともと共有しない（kShareWeight）ので、要素 70 個のリスト P で共有しうる大きさにする。
:- dynamic(m30_n/3).
:- dynamic(m30_nd/1).
test(m30_nan_apart, (m30_upto(1, 70, P), S = g(1.5NaN, P), assertz(m30_n(a, S, S)), \+ m30_n(a, X, X), \+ clause(m30_n(a, Y, Y), true), \+ retract(m30_n(a, Z, Z)), retract(m30_n(a, _, _))), true).
test(m30_nan_deep, (m30_upto(1, 70, P), S = h(g(1.5NaN, P), k), assertz(m30_nd(f(S, S))), \+ m30_nd(f(X, X)), retract(m30_nd(_))), true).
% assert の後で変数が NaN になっても同じ（M30 の Codex レビュー 3 回目）。変数を含む部分項は共有しない。
:- dynamic(m30_nl/1).
:- dynamic(m30_nh/3).
:- dynamic(m30_nr/1).
test(m30_nan_later_body, (m30_upto(1, 70, P), S = g(X, P), assertz((m30_nl(X) :- S = S)), \+ m30_nl(1.5NaN), retract((m30_nl(_) :- _))), true).
test(m30_nan_later_head, (m30_upto(1, 70, P), S = g(X, P), assertz(m30_nh(X, f(S, S), S)), \+ m30_nh(1.5NaN, f(Y, Y), _), \+ retract(m30_nh(1.5NaN, f(Z, Z), _)), retract(m30_nh(_, _, _))), true).
test(m30_nan_after_read, (m30_upto(1, 70, P), S = g(X, P), assertz((m30_nr(X) :- T = S, X = 1.5NaN, T = S)), \+ m30_nr(_), retract((m30_nr(_) :- _))), true).
test(m30_failed_branch,(S = g(1), assertz((m30_re(R) :- (Q = w(S, S), Q = z ; true), R = v(S, S))), m30_re(v(A, B)), A == B, A == g(1)), true).
