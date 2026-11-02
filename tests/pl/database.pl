:- dynamic(pet/1).
pet(cat).

:- dynamic(bug/1).
bug(ant).

:- dynamic(gone/1).
gone(a).

:- dynamic(many/1).
many(a).
many(b).

:- dynamic(allgone/1).
allgone(a).
allgone(b).

:- dynamic(temp/1).
temp(a).

:- dynamic(step/1).
step(1).
step(2).

:- dynamic(seen/1).
seen(1).
seen(2).

:- dynamic(going/1).
going(X) :- retract((going(X) :- _)), still(X).
still(1).

:- dynamic(mix/1).
mix(a).
mix(X) :- mix_body(X).
mix_body(b).

:- dynamic(allrule/1).
allrule(a).
allrule(X) :- allrule_body(X).
allrule_body(b).

:- dynamic(keep/1).
keep(a).
keep(b).

:- dynamic(none/1).

:- dynamic(box/1).

frozen(1).

test(assertz_pet, assertz(pet(dog)), true).
test(pet_both, pet(X), solutions([X = cat, X = dog])).
test(asserta_bug, asserta(bug(gnat)), true).
test(bug_order, bug(X), solutions([X = gnat, X = ant])).
test(assertz_static, assertz(frozen(2)), error(permission_error(modify, static_procedure, frozen/1))).
test(assertz_static_keeps, frozen(X), solutions([X = 1])).
test(asserta_static, asserta(frozen(0)), error(permission_error(modify, static_procedure, frozen/1))).
test(retract_one, retract(gone(a)), true).
test(gone_left, gone(X), fail).
test(retract_many, retract(many(X)), solutions([X = a, X = b])).
test(many_none, many(X), fail).
test(retract_static, retract(frozen(1)), error(permission_error(modify, static_procedure, frozen/1))).
test(retractall_ok, retractall(allgone(_)), true).
test(allgone_none, allgone(X), fail).

:- dynamic(wipe_aa/1).
wipe_aa(1).
wipe_aa(2).
wipe_aa(3).
wipe_aa_all :- retractall(wipe_aa(_)).

test(retractall_in_clause, wipe_aa_all, true).
test(retractall_in_clause_gone, wipe_aa(X), fail).
test(retractall_static, retractall(frozen(_)), error(permission_error(modify, static_procedure, frozen/1))).
test(abolish_ok, abolish(temp/1), true).
test(temp_gone, temp(X), error(existence_error(procedure, temp/1))).
test(assertz_after_abolish, assertz(temp(b)), true).
test(temp_back, temp(X), solutions([X = b])).
test(abolish_static, abolish(frozen/1), error(permission_error(modify, static_procedure, frozen/1))).
test(frozen_remains, frozen(X), solutions([X = 1])).
test(open_misses_asserta, (step(X), (X = 1 -> asserta(step(0)) ; true)), solutions([X = 1, X = 2])).
test(step_now, step(X), solutions([X = 0, X = 1, X = 2])).
test(open_misses_assertz, (seen(X), assertz(seen(9))), solutions([X = 1, X = 2])).
test(seen_later, seen(X), solutions([X = 1, X = 2, X = 9, X = 9])).
test(retract_keeps_body, going(X), solutions([X = 1])).
test(going_gone, going(X), fail).
test(retract_sticks, (retract(keep(a)), fail), fail).
test(keep_left, keep(X), solutions([X = b])).
test(retractall_empty, retractall(none(_)), true).
test(assert_copies, (assertz(box(Z)), Z = 1, box(Y), Y == 1), fail).
test(assertz_var, assertz(X), error(instantiation_error)).
test(assertz_number, assertz(1), error(type_error(callable, 1))).
test(assertz_head_var, assertz((X :- true)), error(instantiation_error)).
test(assertz_body_number, assertz((ok :- 1)), error(type_error(callable, 1))).

:- dynamic(later/1).
later(1).
later(2).
later(3).

:- dynamic(scan_aa/1).
scan_aa(1).
scan_aa(2).
scan_aa(3).

test(logical_update_asserta, (with_output_to(atom(A), ((scan_aa(X), write(X), asserta(scan_aa(4)), asserta(scan_aa(5)), fail) ; true)), A == '123'), true).

% iso-notes 7.5.4 の決定 B（ADR 0008）。開いた走査は、呼んだ時点の節を最後まで試す。新しい呼び出しは消した節を見ない。
test(retract_open_later, (later(X), (X = 1 -> retract(later(3)) ; true)), solutions([X = 1, X = 2, X = 3])).
test(later_left, later(X), solutions([X = 1, X = 2])).

:- dynamic(lu_r/1).
lu_r(1).
lu_r(2).
lu_r(3).
:- dynamic(lu_ra/1).
lu_ra(1).
lu_ra(2).
lu_ra(3).
:- dynamic(lu_cr/1).
lu_cr(1).
lu_cr(2).
lu_cr(3).
:- dynamic(lu_rr/1).
lu_rr(1).
lu_rr(2).
lu_rr(3).
:- dynamic(lu_ab/1).
lu_ab(1).
lu_ab(2).
:- dynamic(lu_dr/1).
lu_dr(1).
lu_dr(2).
lu_dr(3).
:- dynamic(lu_ca/1).
lu_ca(1).
lu_ca(2).
lu_ca(3).
:- dynamic(lu_rt/1).
lu_rt(1).
lu_rt(2).
lu_rt(3).
:- dynamic(lu_nc/1).
lu_nc(1).
lu_nc(2).
lu_nc(3).

test(lu_retract, (with_output_to(atom(A), ((lu_r(X), write(X), retract(lu_r(_)), fail) ; true)), A == '123'), true).
test(lu_retract_left, lu_r(_), fail).
test(lu_retractall, (with_output_to(atom(A), ((lu_ra(X), write(X), retractall(lu_ra(_)), fail) ; true)), A == '123'), true).
test(lu_retractall_left, lu_ra(_), fail).
test(lu_clause_retract, (with_output_to(atom(A), ((clause(lu_cr(X), true), write(X), retract(lu_cr(_)), fail) ; true)), A == '123'), true).
test(lu_clause_left, clause(lu_cr(_), _), fail).
test(lu_abolish, (with_output_to(atom(A), ((lu_rr(X), write(X), abolish(lu_rr/1), fail) ; true)), A == '123'), true).
test(lu_abolish_gone, current_predicate(lu_rr/1), fail).
test(lu_abolish_clause, clause(lu_rr(_), _), fail).
test(lu_abolish_call, lu_rr(_), error(existence_error(procedure, lu_rr/1))).
test(lu_abolish_reassert, (assertz(lu_rr(9)), findall(X, lu_rr(X), L), L == [9]), true).
test(lu_abolish_assert_open, (with_output_to(atom(A), ((lu_ab(X), write(X), (X == 1 -> abolish(lu_ab/1), assertz(lu_ab(7)) ; true), fail) ; true)), A == '12'), true).
test(lu_abolish_assert_left, (findall(X, lu_ab(X), L), L == [7]), true).
% 開いた走査が持つ節を別の retract が先に消したら、やり直した retract はその節を二度は消さない。
test(lu_double_retract, findall(Y, (lu_dr(Z), Z == 1, retract(lu_dr(Y)), (Y == 1 -> retract(lu_dr(2)) ; true)), L), solutions([L = [1, 3]])).
test(lu_double_left, lu_dr(_), fail).
test(lu_count_live, (lu_nc(X), X == 1, retract(lu_nc(3)), predicate_property(lu_nc(_), number_of_clauses(N))), solutions([(X = 1, N = 2)])).
test(lu_clause_abolish, (with_output_to(atom(A), ((clause(lu_ca(X), true), write(X), abolish(lu_ca/1), fail) ; true)), A == '123'), true).
test(lu_clause_abolish_gone, current_predicate(lu_ca/1), fail).
% retract の選択点が残ったまま述語が abolish されたら、やり直した retract は死んだ節を消さずに終わる。
test(lu_retract_abolish, findall(X, (retract(lu_rt(X)), (X == 1 -> abolish(lu_rt/1) ; true)), L), solutions([L = [1]])).
test(lu_retract_abolish_gone, current_predicate(lu_rt/1), fail).

% レビュー M3（ADR 0015）。節の番号を変わらないものにしても、開いた走査の答えは今と同じ。
:- dynamic(m3_ao/1).
m3_ao(1).
m3_ao(2).
m3_ao(3).
:- dynamic(m3_rb/1).
m3_rb(1).
m3_rb(2).
m3_rb(3).
m3_rb(4).
m3_rb(5).

% 開いた走査は asserta した節を見ない。次の呼び出しは新しい節を先頭に見る。
test(m3_asserta_open, (m3_ao(X), (X == 1 -> asserta(m3_ao(4)) ; true)), solutions([X = 1, X = 2, X = 3])).
test(m3_asserta_open_left, m3_ao(X), solutions([X = 4, X = 1, X = 2, X = 3])).
% 走査中の節より前（試し終えた節）と後（まだ試していない節）を消す。前は真ん中から消えるので番号が飛ぶ。
test(m3_retract_open_both, (m3_rb(X), (X == 3 -> retract(m3_rb(2)), retract(m3_rb(5)) ; true)), solutions([X = 1, X = 2, X = 3, X = 4, X = 5])).
test(m3_retract_open_both_left, m3_rb(X), solutions([X = 1, X = 3, X = 4])).

:- dynamic(m3_sc/2).
m3_sc(1, a).
m3_sc(2, a).
m3_sc(3, a).
m3_sc(4, a).
m3_sc(5, a).
m3_sc(6, a).
m3_sc(7, a).
m3_sc(8, a).
m3_sc(9, a).
:- dynamic(m3_sa/2).
m3_sa(1, a).
m3_sa(2, a).
m3_sa(3, a).
:- dynamic(m3_ar/1).
m3_ar(1).
m3_ar(2).
m3_ar(3).
:- dynamic(m3_ac/1).
m3_ac(1).
m3_ac(2).
m3_ac(3).

% 第1引数が未束縛の走査の先の節を消し、後ろへ足す。走査は呼んだ時点の 9 節を試し、次の呼び出しは消した節を見ない。
test(m3_sweep_cursor_retract, (m3_sc(X, _), (X == 1 -> retract(m3_sc(5, _)), assertz(m3_sc(10, a)) ; true)), solutions([X = 1, X = 2, X = 3, X = 4, X = 5, X = 6, X = 7, X = 8, X = 9])).
test(m3_sweep_cursor_retract_left, findall(X, m3_sc(X, _), L), solutions([L = [1, 2, 3, 4, 6, 7, 8, 9, 10]])).
% 同じ走査の中で abolish して足し直す。走査は abolish の前の節を試し、次の呼び出しは足し直した節だけを見る。
test(m3_sweep_abolish_open, (m3_sa(X, _), (X == 1 -> abolish(m3_sa/2), assertz(m3_sa(7, b)) ; true)), solutions([X = 1, X = 2, X = 3])).
test(m3_sweep_abolish_open_left, findall(X, m3_sa(X, _), L), solutions([L = [7]])).
% retract と clause/2 の開いた走査も asserta した節を見ない。clause/2 は連言の中のやり直しに既知の不具合（M18）があるので findall で包む。
test(m3_asserta_open_retract, findall(X, (retract(m3_ar(X)), (X == 1 -> asserta(m3_ar(0)) ; true)), L), solutions([L = [1, 2, 3]])).
test(m3_asserta_open_retract_left, findall(X, m3_ar(X), L), solutions([L = [0]])).
test(m3_asserta_open_clause, findall(X, (clause(m3_ac(X), true), (X == 1 -> asserta(m3_ac(0)) ; true)), L), solutions([L = [1, 2, 3]])).
test(m3_asserta_open_clause_left, findall(X, m3_ac(X), L), solutions([L = [0, 1, 2, 3]])).

% レビュー M3 の第1引数の索引（ADR 0015）。索引は 8 節以上の述語だけが持つので、どの述語も 9 節以上にする。
% 鍵の組と鍵の無い節を合わせても、線形に照らしたときと同じ節を同じ順に返すこと。
:- dynamic(m3_ok/2).
m3_ok(a, 1).
m3_ok(_, 2).
m3_ok(b, 3).
m3_ok(a, 4).
m3_ok(f(x), 5).
m3_ok(_, 6).
m3_ok(b, 7).
m3_ok(a, 8).
m3_ok(_, 9).
m3_ok(c, 10).
:- dynamic(m3_kz/2).
m3_kz(0.0, a).
m3_kz(1, b).
m3_kz(-0.0, c).
m3_kz(0, d).
m3_kz(_, e).
m3_kz(1.0, f).
m3_kz(-0.0, g).
m3_kz(0.0, h).
m3_kz(-1.0, i).
:- dynamic(m3_kn/2).
m3_kn(0.0, a).
m3_kn(_, b).
m3_kn(1.0, c).
m3_kn(_, d).
m3_kn(nan, e).
m3_kn(2.0, f).
m3_kn(_, g).
m3_kn(3.0, h).
% 2^60 からは箱の整数になる。2^61 + 5 は下の桁が 5 と同じ。
:- dynamic(m3_kb/2).
m3_kb(5, a).
m3_kb(1152921504606846975, b).
m3_kb(1152921504606846976, c).
m3_kb(2305843009213693957, d).
m3_kb(_, e).
m3_kb(9223372036854775807, f).
m3_kb(-1152921504606846977, g).
m3_kb(-1152921504606846976, h).
m3_kb(1152921504606846976, i).
m3_kb(5.0, j).
% 4607182418800017408 は 1.0 のビット列と同じ値の整数。'1' はアトム。
:- dynamic(m3_kf/2).
m3_kf(1, a).
m3_kf(1.0, b).
m3_kf(4607182418800017408, c).
m3_kf(_, d).
m3_kf(2, e).
m3_kf(2.0, f).
m3_kf(1, g).
m3_kf(1.0, h).
m3_kf('1', i).
:- dynamic(m3_ka/2).
m3_ka(f, a).
m3_ka(f(1), b).
m3_ka(f(1, 2), c).
m3_ka(g(1), d).
m3_ka(_, e).
m3_ka(f(2), f).
m3_ka(f(x, y, z), g).
m3_ka(f, h).
m3_ka(f(3, 4), i).
:- dynamic(m3_kl/2).
m3_kl([], a).
m3_kl([x], b).
m3_kl('[]', c).
m3_kl([y | z], d).
m3_kl(_, e).
m3_kl('.'(p, q), f).
m3_kl({}, g).
m3_kl([], h).
m3_kl([[]], i).
% 7 節。索引を作る数の手前。
:- dynamic(m3_ta/2).
m3_ta(k, 1).
m3_ta(_, 2).
m3_ta(k, 3).
m3_ta(_, 4).
m3_ta(k, 5).
m3_ta(_, 6).
m3_ta(k, 7).

% 鍵の無い節は鍵の組の間に節の順で入る。鍵の組が無い呼び出しと、鍵の型が違う呼び出しは鍵の無い節だけを見る。
test(m3_order_keyless, m3_ok(a, X), solutions([X = 1, X = 2, X = 4, X = 6, X = 8, X = 9])).
test(m3_order_keyless_absent, findall(X, m3_ok(z, X), L), solutions([L = [2, 6, 9]])).
test(m3_order_keyless_type, findall(X, m3_ok(1, X), L), solutions([L = [2, 6, 9]])).
test(m3_order_keyless_struct, findall(X, m3_ok(f(_), X), L), solutions([L = [2, 5, 6, 9]])).
% 0.0 と -0.0 は同じ鍵（単一化も通る）。整数の 0 とは別の鍵。
test(m3_keys_zero, findall(R, m3_kz(0.0, R), L), solutions([L = [a, c, e, g, h]])).
test(m3_keys_zero_negative, findall(R, (Z is -(0.0), m3_kz(Z, R)), L), solutions([L = [a, c, e, g, h]])).
test(m3_keys_zero_int, findall(R, m3_kz(0, R), L), solutions([L = [d, e]])).
% NaN の鍵はどの NaN とも同じ鍵だが、NaN は単一化しないので鍵の無い節だけが答える。アトム nan は別の鍵。
test(m3_keys_nan_setup, (N is nan, M is -(nan), assertz(m3_kn(N, i)), assertz(m3_kn(M, j)), assertz(m3_kn(_, k))), true).
test(m3_keys_nan, findall(R, (N is nan, m3_kn(N, R)), L), solutions([L = [b, d, g, k]])).
test(m3_keys_nan_negative, findall(R, (N is -(nan), m3_kn(N, R)), L), solutions([L = [b, d, g, k]])).
test(m3_keys_nan_atom, findall(R, m3_kn(nan, R), L), solutions([L = [b, d, e, g, k]])).
test(m3_keys_nan_all, findall(R, m3_kn(_, R), L), solutions([L = [a, b, c, d, e, f, g, h, i, j, k]])).
% 箱の整数も即値の整数も値で引く。計算した箱の整数と、読んだ箱の整数は同じ鍵。
test(m3_keys_big, findall(R, (K is 1 << 60, m3_kb(K, R)), L), solutions([L = [c, e, i]])).
test(m3_keys_big_literal, findall(R, m3_kb(1152921504606846976, R), L), solutions([L = [c, e, i]])).
test(m3_keys_big_below, findall(R, (K is (1 << 60) - 1, m3_kb(K, R)), L), solutions([L = [b, e]])).
test(m3_keys_big_low_bits, findall(R, m3_kb(5, R), L), solutions([L = [a, e]])).
test(m3_keys_big_max, findall(R, (K is 9223372036854775807, m3_kb(K, R)), L), solutions([L = [e, f]])).
test(m3_keys_big_negative, findall(R, (K is -(1 << 60) - 1, m3_kb(K, R)), L), solutions([L = [e, g]])).
test(m3_keys_big_negative_small, findall(R, (K is -(1 << 60), m3_kb(K, R)), L), solutions([L = [e, h]])).
% 整数と浮動小数点数は値が同じでも、ビット列が同じでも別の鍵。アトム '1' も別。
test(m3_keys_int_float, findall(R, m3_kf(1, R), L), solutions([L = [a, d, g]])).
test(m3_keys_int_float_float, findall(R, m3_kf(1.0, R), L), solutions([L = [b, d, h]])).
test(m3_keys_int_float_bits, findall(R, m3_kf(4607182418800017408, R), L), solutions([L = [c, d]])).
test(m3_keys_int_float_atom, findall(R, m3_kf('1', R), L), solutions([L = [d, i]])).
% 複合項の鍵は名前とアリティ。同じ名前のアトムとも別。
test(m3_keys_arity, findall(R, m3_ka(f(_), R), L), solutions([L = [b, e, f]])).
test(m3_keys_arity_two, findall(R, m3_ka(f(_, _), R), L), solutions([L = [c, e, i]])).
test(m3_keys_arity_three, findall(R, m3_ka(f(_, _, _), R), L), solutions([L = [e, g]])).
test(m3_keys_arity_atom, findall(R, m3_ka(f, R), L), solutions([L = [a, e, h]])).
test(m3_keys_arity_name, findall(R, m3_ka(g(_), R), L), solutions([L = [d, e]])).
% [] はアトムで '[]' と同じ。リストの鍵は中身に依らずどのリストとも合う。
test(m3_keys_nil_list, findall(R, m3_kl([], R), L), solutions([L = [a, c, e, h]])).
test(m3_keys_nil_list_cons, findall(R, m3_kl([_ | _], R), L), solutions([L = [b, d, e, f, i]])).
test(m3_keys_nil_list_curly, findall(R, m3_kl({}, R), L), solutions([L = [e, g]])).
% 7 節の走査を開いたまま 2 節足す。走査は 7 節を見て、次の呼び出しは 9 節（索引を作る数を越えた）を見る。
test(m3_threshold_assertz, (m3_ta(k, X), (X == 1 -> assertz(m3_ta(k, 8)), assertz(m3_ta(_, 9)) ; true)), solutions([X = 1, X = 2, X = 3, X = 4, X = 5, X = 6, X = 7])).
test(m3_threshold_assertz_left, findall(X, m3_ta(k, X), L), solutions([L = [1, 2, 3, 4, 5, 6, 7, 8, 9]])).
test(m3_threshold_assertz_keyless, findall(X, m3_ta(j, X), L), solutions([L = [2, 4, 6, 9]])).
% 索引を作った後の asserta と assertz も、開いた走査は見ず、次の呼び出しは見る。
test(m3_index_extend, (asserta(m3_ta(j, 0)), assertz(m3_ta(j, 10)), findall(X, m3_ta(j, X), L)), solutions([L = [0, 2, 4, 6, 9, 10]])).
test(m3_index_extend_open, (m3_ta(j, X), (X == 0 -> asserta(m3_ta(j, -1)), assertz(m3_ta(j, 11)) ; true)), solutions([X = 0, X = 2, X = 4, X = 6, X = 9, X = 10])).
test(m3_index_extend_left, findall(X, m3_ta(j, X), L), solutions([L = [-1, 0, 2, 4, 6, 9, 10, 11]])).
% 先頭、末尾、真ん中の節を消す。消した節は次の呼び出しに出ない。
test(m3_index_erase, (retract(m3_ta(j, -1)), retract(m3_ta(j, 11)), retract(m3_ta(k, 5)), findall(X, m3_ta(j, X), J), findall(X, m3_ta(k, X), K)), solutions([(J = [0, 2, 4, 6, 9, 10], K = [1, 2, 3, 4, 6, 7, 8, 9])])).
% 索引の組の両端を外す。前から 9 節消すと組の始まりが半分を越えて詰め直し、後ろから消し尽くすと asserta した番号を
% 後ろから外す。どれも呼び出しの時点で 8 節以上あり、索引を引く。
test(m3_bucket_pop_front, (m3_qe_fill(1, 16), findall(X, m3_qe(k, X), _), m3_qe_drop(1, 9), m3_qe_fill(17, 18), findall(X, m3_qe(k, X), L)), solutions([L = [10, 11, 12, 13, 14, 15, 16, 17, 18]])).
test(m3_bucket_pop_back, (asserta(m3_qe(k, 9)), asserta(m3_qe(k, 8)), asserta(m3_qe(k, 7)), m3_qe_drop_down(18, 9), m3_qe_fill(19, 25), findall(X, m3_qe(k, X), L)), solutions([L = [7, 8, 19, 20, 21, 22, 23, 24, 25]])).
test(m3_bucket_after_move, (asserta(m3_qe(k, 6)), retract(m3_qe(k, 6)), asserta(m3_qe(k, 5)), findall(X, m3_qe(k, X), L)), solutions([L = [5, 7, 8, 19, 20, 21, 22, 23, 24, 25]])).
% 読み直しは節を置き換える。前の読み込みの鍵の組を引かないこと。ctest は prolog_tests と差分テストを同時に流すので、
% 中身ごとに別のファイルにし、同じパスへ違う中身を書き合わないようにする。
test(m3_reconsult_first, (m3_write_clauses('tests/tmp/m3_reconsult_1.pl', [m3_rc(a, 1), m3_rc(b, 2), m3_rc(_, 3), m3_rc(c, 4), m3_rc(a, 5), m3_rc(d, 6), m3_rc(e, 7), m3_rc(c, 8), m3_rc(f, 9)]), consult('tests/tmp/m3_reconsult_1.pl'), findall(X, m3_rc(c, X), L)), solutions([L = [3, 4, 8]])).
test(m3_reconsult, (m3_write_clauses('tests/tmp/m3_reconsult_2.pl', [m3_rc(c, 10), m3_rc(g, 11), m3_rc(h, 12), m3_rc(_, 13), m3_rc(i, 14), m3_rc(j, 15), m3_rc(c, 16), m3_rc(k, 17), m3_rc(l, 18)]), consult('tests/tmp/m3_reconsult_2.pl'), findall(X, m3_rc(c, X), C), findall(X, m3_rc(a, X), A)), solutions([(C = [10, 13, 16], A = [13])])).

:- dynamic(m3_qe/2).
m3_qe_fill(I, N) :- I > N, !.
m3_qe_fill(I, N) :- assertz(m3_qe(k, I)), I1 is I + 1, m3_qe_fill(I1, N).
m3_qe_drop(I, N) :- I > N, !.
m3_qe_drop(I, N) :- retract(m3_qe(k, I)), I1 is I + 1, m3_qe_drop(I1, N).
m3_qe_drop_down(I, N) :- I < N, !.
m3_qe_drop_down(I, N) :- retract(m3_qe(k, I)), I1 is I - 1, m3_qe_drop_down(I1, N).
% 第1引数で節を選ぶので、選択点を残さない。読み込みは選択点が残っていると述語を置き換えない。
m3_write_clauses(Path, Clauses) :- open(Path, write, S), m3_put_clauses(Clauses, S), close(S).
m3_put_clauses([], _).
m3_put_clauses([C | Cs], S) :- writeq(S, C), write(S, '.'), nl(S), m3_put_clauses(Cs, S).
test(assertz_builtin, assertz(true), error(permission_error(modify, static_procedure, true/0))).
test(retract_builtin, retract(fail), error(permission_error(modify, static_procedure, fail/0))).
test(abolish_builtin, abolish(atom/1), error(permission_error(modify, static_procedure, atom/1))).
test(assertz_builtin_is, assertz(is(1, 1)), error(permission_error(modify, static_procedure, is/2))).
test(retract_fact_only, (retract(mix(Y)), \+ retract(mix(_))), solutions([Y = a])).
test(mix_rule_left, mix(Y), solutions([Y = b])).
test(retractall_rules, retractall(allrule(_)), true).
test(allrule_none, allrule(X), fail).
test(assertz_throw, assertz(throw(1)), error(permission_error(modify, static_procedure, throw/1))).
test(abolish_catch, abolish(catch/3), error(permission_error(modify, static_procedure, catch/3))).
test(abolish_comma, abolish((',')/2), error(permission_error(modify, static_procedure, (',')/2))).
test(assertz_arrow, assertz((a -> b)), error(permission_error(modify, static_procedure, (->)/2))).
% (:-)/1 は指示、(:-)/2 は節の首。組込表に無くても静的なので asserta は足せない。
test(asserta_neck_2, asserta(((a :- b) :- true)), error(permission_error(modify, static_procedure, (:-)/2))).
test(asserta_neck_1, asserta(((:- b) :- true)), error(permission_error(modify, static_procedure, (:-)/1))).
test(assert_nested, assertz((nest_phase3 :- (true, 4))), error(type_error(callable, (true, 4)))).
test(assert_builtin_body, assertz((atom(_) :- (true, 4))), error(type_error(callable, (true, 4)))).

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
