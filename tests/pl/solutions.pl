d(1, a).
d(2, b).
d(3, a).

test(findall_two, findall(X, (X = 1; X = 2), S), solutions([S = [1, 2]])).
test(findall_none, findall(X, fail, S), solutions([S = []])).
test(findall_copy, (findall(f(X), (X = 1; X = 2), L), X = 3, L == [f(1), f(2)]), true).
test(findall_throw, findall(X, throw(error(instantiation_error, _)), S), error(instantiation_error)).
test(findall_var_goal, findall(X, Y, S), error(instantiation_error)).
test(bagof_none, bagof(X, fail, S), fail).
test(bagof_dup, bagof(X, (X = b; X = a; X = b), L), solutions([L = [b, a, b]])).
test(bagof_group, bagof(X, d(X, Y), L), solutions([(Y = a, L = [1, 3]), (Y = b, L = [2])])).
test(bagof_exist, bagof(X, Y^d(X, Y), L), solutions([L = [1, 2, 3]])).
test(bagof_exist_copy, (
  findall(Ls, bagof(L, A^B^C^(L = [A, B, C], bagof(t, (E = A; E = B; E = C), _)), Ls), All),
  All = [Ls],
  Ls = [[E, P, Q], [R, E, S], [T, U, E]],
  P \== E, Q \== E, R \== E, S \== E, T \== E, U \== E,
  P \== Q, P \== R, P \== S, P \== T, P \== U,
  Q \== R, Q \== S, Q \== T, Q \== U,
  R \== S, R \== T, R \== U,
  S \== T, S \== U,
  T \== U
), true).
test(bagof_free_alias, (
  findall(1, bagof(E, bagof(t, (A = E; B = E; C = E), _), _), N),
  N = [1],
  bagof(E, bagof(t, (A = E; B = E; C = E), _), L),
  (L == [A, B, C]; L == [C, B, A]),
  bagof(X, (Y = X; Z = X), S),
  S == [Y, Z]
), true).
test(setof_sort, setof(X, (X = b; X = a; X = b), L), solutions([L = [a, b]])).
test(setof_group, setof(X, d(X, Y), L), solutions([(Y = a, L = [1, 3]), (Y = b, L = [2])])).
test(setof_exist, setof(X, Y^d(X, Y), L), solutions([L = [1, 2, 3]])).
% 先頭の ^ の連なりが巡回すれば resource_error(cyclic_term)（iso-notes 7.3.3）。前は strip_caret が
% 回り続け、--heap-max でも止まらなかった（レビュー、2026-11-17）。
test(bagof_caret_cycle, (X = (_^X), bagof(a, X, _)), error(resource_error(cyclic_term))).
test(bagof_caret_cycle_two, (X = (_^Y), Y = (_^X), bagof(a, X, _)), error(resource_error(cyclic_term))).
test(setof_caret_cycle, (X = (_^X), setof(a, X, _)), error(resource_error(cyclic_term))).
% 巡回の前に巡回しない 2 層があり周期が 3 なので、Brent の見張りが先へ動いてから戻りを見つける。
test(bagof_caret_cycle_prefix, (Y = (_^(_^(_^Y))), X = (_^(_^Y)), bagof(a, X, _)),
     error(resource_error(cyclic_term))).
% ゴールの検査は第3引数の検査より先（有限の bagof(a, _^1, foo) で type_error(callable, 1) が先に出るのと同じ順）。
test(bagof_caret_cycle_list, (X = (_^X), bagof(a, X, foo)), error(resource_error(cyclic_term))).
% 巡回しない 3 層の連なりを巡回と取り違えない。
test(setof_caret_chain, setof(X, Y^Z^W^(d(X, Y), Z = W), L), solutions([L = [1, 2, 3]])).

q(A, _, A).
q(_, A, A).
q(A, _, A).

:- dynamic(a/1).
a(1).
a(2).

:- dynamic(r/1).
r(1).
r(2).
r(3).

test(bagof_share, (bagof(f(X), (X = Y; X = Z), S), Y = a, S == [f(a), f(Z)]), true).
test(setof_same_var, (setof(X, (X = Y; X = Y), L), L == [Y]), true).
test(bagof_rename, bagof(none, q(X, Y, Z), L), solutions([(X = Z, L = [none, none]), (Y = Z, L = [none])])).
% 証人の鍵の印と同じ形の利用者の項は、ゴールが作った変数と別の組になる（8.10.2.1）。
pm(1, f('$rename'(0))).
pm(2, f(_)).
pm(3, f('$quote'('$rename'(0)))).
% '$quote' も包む。包まないと pm(4) の鍵が pm(1) を包んだ鍵と同じ f('$quote'('$rename'(0))) になる。
pm(4, f('$quote'(_))).
test(bagof_marker_shaped, (findall(L, bagof(X, pm(X, _), L), Ls), Ls == [[1], [2], [3], [4]]), true).
test(scan_asserta, (a(X), findall(_, asserta(a(9)), _)), solutions([X = 1, X = 2])).
test(a_now, a(X), solutions([X = 9, X = 9, X = 1, X = 2])).
% ADR 0008。開いた走査は消した r(2) も試す。新しい呼び出しの r_left は見ない。
test(scan_retract, (r(X), findall(_, (X = 1 -> retract(r(2)) ; true), _)), solutions([X = 1, X = 2, X = 3])).
test(r_left, r(X), solutions([X = 1, X = 3])).
test(findall4_closed, findall(X, (X = 1; X = 2), L, []), solutions([L = [1, 2]])).
test(findall4_none, (findall(X, fail, L, T), L == T, var(T)), true).
test(findall4_open, (findall(X, (X = 1; X = 2), L, T), L == [1, 2|T], var(T)), true).
test(findall4_throw, findall(X, throw(error(instantiation_error, _)), L, T), error(instantiation_error)).
test(findall4_var_goal, findall(X, Y, L, T), error(instantiation_error)).
test(findall4_tail_improper, findall(X, X=1, _, [A|1]), error(type_error(list, [A|1]))).
test(findall4_tail_int, findall(X, (X=2; X=1), _, 12), error(type_error(list, 12))).
test(findall3_still, findall(X, (X = 1; X = 2), S), solutions([S = [1, 2]])).

:- dynamic(scan_p/1).
scan_p(a).
scan_p(b).
scan_p(c).

test(findall_throw_keeps_scan, (
  clause(scan_p(X), true),
  catch(findall(_, (asserta(scan_p(z)), throw(error(instantiation_error, _))), _, _),
        error(instantiation_error, _),
        true)
), solutions([X = a, X = b, X = c])).

% ADR 0013 の見張り。収集を呼び手のループの中で動かしても、答え・カット・例外・副作用の順序は変わらない。
:- dynamic(lv_q/1).

test(collect_cut_local, findall(X, ((X = 1; X = 2), !), L), solutions([L = [1]])).
test(collect_cut_after, ((Y = 1; Y = 2), findall(X, (X = a; X = b), L), !), solutions([(Y = 1, L = [a, b])])).
test(collect_in_not, \+ findall(_, fail, [_]), true).
test(collect_in_recovery, catch(throw(b), b, findall(X, (X = 1; X = 2), L)), solutions([L = [1, 2]])).
test(collect_throw_keeps_outer, ((Y = 1; Y = 2), catch(findall(_, throw(t), _), t, true)), solutions([Y = 1, Y = 2])).
test(collect_catch_inside, findall(X, catch((X = 1; X = 2; throw(s)), s, X = 3), L), solutions([L = [1, 2, 3]])).
test(collect_nested_redo, findall(X-L, ((X = 1; X = 2), findall(Y, (Y = X; Y = 0), L)), R),
     solutions([R = [1-[1, 0], 2-[2, 0]]])).
test(collect_bagof_groups_then_findall, (bagof(X, d(X, Y), L), findall(Z, d(Z, Y), M)),
     solutions([(Y = a, L = [1, 3], M = [1, 3]), (Y = b, L = [2], M = [2])])).
test(collect_bagof_of_findall, bagof(Y-L, ((Y = a; Y = b), findall(X, d(X, Y), L)), R),
     solutions([R = [a-[1, 3], b-[2]]])).
test(collect_update_view, (retractall(lv_q(_)), assertz(lv_q(a)), assertz(lv_q(b)),
                           findall(X, (lv_q(X), assertz(lv_q(n))), L)), solutions([L = [a, b]])).

% 8.10.2.1 と 8.10.3.1。反復するゴールは findall/3 で写すので、呼ぶ前からあった Template の変数も解ごとに新しい
% （レビュー 2026-11-20 の C-02）。前は ^ の下に無い呼ぶ前からの変数を全部、呼び手のセルのまま返した。
% 呼び手のセルのまま返すのは証人の変数だけ（bagof_free_alias が守る）。
test(c02_self_list, (bagof(X, true, X), X = [Y], var(Y)), true).
test(c02_fresh_each, (bagof(A, (true; true), [P, Q]), P \== Q, P \== A, Q \== A), true).
test(c02_setof_two, (setof(B, (true; true), L), L = [_, _]), true).
test(c02_later_bindings, (bagof(f(C, D), (C = a; D = b), L), C = 1, D = 2, L = [f(a, P), f(Q, b)], var(P), var(Q)),
     true).
test(c02_caret_both, (bagof(X, Y^(Y = X; Y = X), [P, Q]), P \== Q, P \== X), true).
test(c02_caret_flip, (bagof(X, Y^(X = Y; X = Y), [P, Q]), P \== Q, P \== X), true).
test(c02_pair_fresh, (bagof(X-Y, (X = 1; X = 2), [1-P, 2-Q]), P \== Q, P \== Y, Q \== Y), true).
% 証人 Y の具体化 f(T) と f(A) は変種なので 1 つの組になる。Y は 1 つ目の T と同じ変数で、2 つ目の T は新しい。
test(c02_caret_group, (findall(Y-L, bagof(T, A^(Y = f(T); Y = f(A)), L), R), R = [f(P)-[Q, S]],
                       P == Q, var(S), S \== P), true).
% 守り。証人 U が自由なままの w(f(T), U) と w(f(U), U) は変種でないので、2 つの組のまま。
test(c02_free_witness_two, (findall(Y-L, bagof(T, (Y = f(T); Y = f(U)), L), R), R = [_, _], var(U)), true).

% 8.10.2.1 と 8.10.3.1。証人のセル同士が結び付いたとき、どちらの向きで結び付いても w(V, V) は互いに変種なので、
% 1 つの組になる（レビュー 2026-11-20 の C-03）。前は結び付いた先のセルで番号を振ったので、Y = Z と Z = Y で
% 鍵が分かれた。今の (=)/2 は左を右へ結ぶので、両方の向きを 1 つのゴールに置く。L-06 で古いセルへ結ぶ向きに
% 揃えると、この再現は向きを作れなくなるので、L-06 より先にここで押さえる。
test(c03_setof_both, (findall(Y-Z-L, setof(1, (Y = Z; Z = Y), L), R), R = [P-Q-[1]], P == Q, var(P)), true).
test(c03_bagof_both, (findall(Y-Z-L, bagof(1, (Y = Z; Z = Y), L), R), R = [P-Q-[1, 1]], P == Q, var(P)), true).
test(c03_bagof_flip, (findall(Y-Z-L, bagof(1, (Z = Y; Y = Z), L), R), R = [P-Q-[1, 1]], P == Q, var(P)), true).
test(c03_three, (findall(X-Y-Z-L, bagof(1, (X = Y, Y = Z; Z = Y, Y = X; Y = X, Z = Y), L), R),
                 R = [P-Q-S-[1, 1, 1]], P == Q, Q == S, var(P)), true).
test(c03_template, (findall(Y-Z-L, bagof(T, (Y = Z, T = Y; Z = Y, T = Z), L), R), R = [P-Q-[A, B]],
                    P == Q, A == P, B == P, var(P)), true).
test(c03_via_local, (findall(Y-Z-L, bagof(1, V^(Y = V, V = Z; Z = V, V = Y), L), R), R = [P-Q-[1, 1]], P == Q),
     true).
test(c03_answer_binds, (bagof(1, (Y = Z; Z = Y), L), L == [1, 1], Y == Z, var(Y)), true).
% 守り。同じ向きの 2 つは前から 1 つの組。w(V, V) と w(V, U) は変種でないので、2 つの組のまま。
test(c03_same_way, (findall(Y-Z-L, bagof(1, (Y = Z; Y = Z), L), R), R = [P-Q-[1, 1]], P == Q), true).
test(c03_apart, (findall(Y-Z-L, bagof(1, (Y = Z; true), L), R), R = [P-Q-[1], S-U-[1]], P == Q, S \== U), true).
% 鍵だけでなく Template の像も同じ証人の番号で写す。鍵だけを揃えると組は 1 つになるが、並べて重なりを除くのは
% 証人を束縛する前なので、f(Y) と f(Z) が 2 つ残った。
test(c03_setof_template, (findall(Y-Z-L, setof(T, (Y = Z, T = f(Y); Z = Y, T = f(Y)), L), R), R = [P-Q-[f(S)]],
                          P == Q, S == P, var(P)), true).
