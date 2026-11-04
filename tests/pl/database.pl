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
% retract と clause/2 の開いた走査も asserta した節を見ない。
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
% 読み直しは節を置き換える。前の読み込みの鍵の組を引かないこと。prolog_tests と差分テストはどちらもこのファイルを
% 書くので、tests.cmake の RESOURCE_LOCK pl_tmp で同時に走らせない。中身ごとに別のファイルにしておく。
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

% レビュー M3 の retract・retractall・clause/2（ADR 0015）。第1引数で索引を引き、真ん中の節をまとめて掃除しても、
% 答えは今と同じ。
:- dynamic(m3_cq/1).
m3_cq(1).
m3_cq(2).
m3_cq(3).
m3_cq(4).
% X は呼び出しの後にできた変数。clause/2 が選択点の水位で束縛を戻さないと、やり直しで最初の答えが残る（レビュー M18）。
m3_cq_show :- clause(m3_cq(X), true), write(X), X > 2, nl.
:- dynamic(m3_sh/2).
m3_sh(1, a).
m3_sh(2, a).
m3_sh(3, a).
m3_sh(4, a).
m3_sh(5, a).
m3_sh(6, a).
m3_sh(7, a).
m3_sh(8, a).
m3_sh(9, a).
m3_sh(10, a).
:- dynamic(m3_rh/1).
m3_rh(1).
m3_rh(2).
m3_rh(3).
m3_rh(4).
m3_rh(5).
m3_rh(6).
m3_rh(7).
m3_rh(8).
m3_rh(9).
m3_rh(10).
:- dynamic(m3_rr/1).
m3_rr(1).
m3_rr(2).
m3_rr(3).
m3_rr(4).
m3_rr(5).
m3_rr(6).
m3_rr(7).
m3_rr(8).
m3_rr(9).
m3_rr(10).
m3_rr(11).
m3_rr(12).
m3_rr(13).
m3_rr(14).
m3_rr(15).
m3_rr(16).
m3_rr_drop(I, N) :- I > N, !.
m3_rr_drop(I, N) :- retract(m3_rr(I)), I1 is I + 1, m3_rr_drop(I1, N).
:- dynamic(m3_so/1).
m3_so(1).
m3_so(2).
m3_so(3).
m3_so(4).
m3_so(5).
:- dynamic(m3_sq/1).
m3_sq(1).
m3_sq(2).
m3_sq(3).
m3_sq(4).
m3_sq(5).
:- dynamic(m3_sl/2).
m3_sl(k, 1).
m3_sl(k, 2).
m3_sl(k, 3).
m3_sl(k, 4).
m3_sl(k, 5).
m3_sl(k, 6).
m3_sl(k, 7).
m3_sl(k, 8).
m3_sl(k, 9).
m3_sl_drop(I, N) :- I > N, !.
m3_sl_drop(I, N) :- retract(m3_sl(k, I)), I1 is I + 1, m3_sl_drop(I1, N).
:- dynamic(m3_ct/2).
m3_ct(1, a).
m3_ct(2, a).
m3_ct(3, a).
m3_ct(4, a).
m3_ct(5, a).
m3_ct(6, a).
m3_ct(7, a).
m3_ct(8, a).
m3_ct(9, a).

test(m3_clause_conj_redo, (with_output_to(atom(A), m3_cq_show), A == '123\n'), true).

% current_predicate/1 も選択点を積む前に束縛する。節の中でできた変数は上の選択点より新しく、束縛がトレイルに
% 残らないので、選択点の水位で戻さないと、やり直しで最初の述語が残り、連言の後ろが落ちる（レビュー M18）。
% 試験のゴールの変数は runner の選択点より古く、旧コードでも戻るので、どれも節の本体で変数を作る。
% アリティ 9 の述語はこのファイルではこの 3 つだけ。
m18_d1(1, 2, 3, 4, 5, 6, 7, 8, 9).
m18_d2(1, 2, 3, 4, 5, 6, 7, 8, 9).
m18_d3(1, 2, 3, 4, 5, 6, 7, 8, 9).
m18_v(1).
m18_v(1, 2).
m18_v(1, 2, 3).
m18_cp_show :- current_predicate(X/9), write(X), X == m18_d3, nl.
m18_cp_name :- current_predicate(X/9), X == m18_d3.
m18_cp_arity :- current_predicate(m18_v/A), A == 3.
m18_cp_indicator :- current_predicate(P), P == m18_d3/9.
m18_cp_qualified :- current_predicate(user:X/9), X == m18_d3.
m18_cp_any_module :- current_predicate(M:X/9), M == user, X == m18_d3.
m18_cp_gen(X) :- current_predicate(Y/9), X = Y.

test(m18_current_conj_redo, (with_output_to(atom(A), m18_cp_show), A == 'm18_d1m18_d2m18_d3\n'), true).
test(m18_current_name_redo, m18_cp_name, true).
test(m18_current_arity_redo, m18_cp_arity, true).
% 束縛していない P は旧コードでも偶然通る。やり直しが、残った束縛の指す番地に次の述語表示を組み直すから。
test(m18_current_indicator_redo, m18_cp_indicator, true).
test(m18_current_qualified_redo, m18_cp_qualified, true).
test(m18_current_any_module_redo, m18_cp_any_module, true).
% findall が答えを捨てて戻るたびに、前の答えの束縛を外してから次の述語と単一化する。
test(m18_current_all_answers, findall(X, m18_cp_gen(X), L), solutions([L = [m18_d1, m18_d2, m18_d3]])).

% 走査を始めた後に消した述語は答えず、作った述語も答えない。候補が述語表の添字だと、入れ子の実行
% （with_output_to/2 と consult の指令）が終わってスタックを戻すとき、消した分のずらしが捨てられ、添字が
% 1 つ後ろの述語を指す（M18 の敵対的レビュー A1）。アリティは試験ごとに分ける。
:- dynamic(m18_e1/12). :- dynamic(m18_e2/12). :- dynamic(m18_e3/12).
m18_e1(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12).
m18_e2(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12).
m18_e3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12).
test(m18_current_abolish_ahead, findall(X, (current_predicate(X/12), (X == m18_e1 -> abolish(m18_e2/12) ; true)), L), solutions([L = [m18_e1, m18_e3]])).
:- dynamic(m18_w1/8). :- dynamic(m18_w2/8). :- dynamic(m18_w3/8). :- dynamic(m18_w4/8).
m18_w1(1, 2, 3, 4, 5, 6, 7, 8).
m18_w2(1, 2, 3, 4, 5, 6, 7, 8).
m18_w3(1, 2, 3, 4, 5, 6, 7, 8).
m18_w4(1, 2, 3, 4, 5, 6, 7, 8).
m18_w_abolish :- abolish(m18_w1/8), assertz(m18_w5(1, 2, 3, 4, 5, 6, 7, 8)).
test(m18_current_nested_abolish, findall(X, (current_predicate(X/8), (X == m18_w1 -> with_output_to(atom(_), m18_w_abolish) ; true)), L), solutions([L = [m18_w1, m18_w2, m18_w3, m18_w4]])).
:- dynamic(m18_c1/10). :- dynamic(m18_c2/10). :- dynamic(m18_c3/10). :- dynamic(m18_c4/10).
m18_c1(1, 2, 3, 4, 5, 6, 7, 8, 9, 10).
m18_c2(1, 2, 3, 4, 5, 6, 7, 8, 9, 10).
m18_c3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10).
m18_c4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10).
m18_c_file('tests/tmp/m18_abolish.pl').
m18_c_consult :- m18_c_file(Path), consult(Path).
m18_c_write :- m18_c_file(Path), open(Path, write, S), write(S, ':- abolish(m18_c1/10).\n'), close(S).
test(m18_current_consult_abolish, (m18_c_write, findall(X, (current_predicate(X/10), (X == m18_c1 -> m18_c_consult ; true)), L)), solutions([L = [m18_c1, m18_c2, m18_c3, m18_c4]])).
% 開いた節の走査があると、abolish した述語は abolished の印で表に残る。新しい呼び出しと同じく、やり直しも
% それを答えない（ADR 0008）。
:- dynamic(m18_a1/11). :- dynamic(m18_a2/11). :- dynamic(m18_a3/11).
m18_a1(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11).
m18_a2(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11).
m18_a2(2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11).
m18_a3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11).
m18_a_open(K) :- m18_a2(K, _, _, _, _, _, _, _, _, _, _).
% 指示が述語を消してから読み込みが失敗しても、巻き戻しは読み込みの前の述語を引けるままにし、読み込みで作った
% 述語を残さない。表の長さへ縮めていたので、末尾に空の述語が足されて serial の昇順が崩れ、最後に作った述語が
% current_predicate/1 から消えた（M18 の Codex レビュー P2）。指示の abolish は実行時の変更なので戻さない。
:- dynamic(m18_r_zap/0).
m18_r_zap.
m18_r_file('tests/tmp/m18_rollback.pl').
m18_r_write :- m18_r_file(Path), open(Path, write, S), write(S, ':- abolish(m18_r_zap/0).\nm18_r_new.\n:- fail.\n'), close(S).
m18_r_rejected :- m18_r_file(Path), \+ consult(Path).
test(m18_rollback_after_abolish, (assertz(m18_r_late), m18_r_write, m18_r_rejected, current_predicate(m18_r_late/0), \+ current_predicate(m18_r_new/0), \+ current_predicate(m18_r_zap/0)), true).
test(m18_current_abolished_kept,findall(X, (m18_a_open(K), K == 1, current_predicate(X/11), (X == m18_a1 -> abolish(m18_a2/11) ; true)), L), solutions([L = [m18_a1, m18_a3]])).
% consult の指示（と初期化のゴール）は、外の走査の選択点を脇に置いて動く。脇に置いた走査も開いた走査なので、
% 指示が消した節を試し、直接呼んだときと同じ答えになる（7.5.4、レビュー M19）。指示のファイルは tests/tmp に書く。
m19_write(Path, Text) :- open(Path, write, S), write(S, Text), close(S).
:- dynamic(m19_p/1).
m19_p(1). m19_p(2). m19_p(3).
test(m19_parked_call, (m19_write('tests/tmp/m19_call.pl', ':- retract(m19_p(3)).\n'), findall(X, (m19_p(X), (X =:= 1 -> consult('tests/tmp/m19_call.pl') ; true)), L), findall(Y, m19_p(Y), L2)), solutions([(L = [1, 2, 3], L2 = [1, 2])])).
:- dynamic(m19_c/1).
m19_c(1). m19_c(2). m19_c(3).
test(m19_parked_clause, (m19_write('tests/tmp/m19_clause.pl', ':- retract(m19_c(3)).\n'), findall(X, (clause(m19_c(X), true), (X =:= 1 -> consult('tests/tmp/m19_clause.pl') ; true)), L)), solutions([L = [1, 2, 3]])).
:- dynamic(m19_i/1).
m19_i(1). m19_i(2). m19_i(3).
test(m19_parked_init, (m19_write('tests/tmp/m19_init.pl', ':- initialization(retract(m19_i(3))).\n'), findall(X, (m19_i(X), (X =:= 1 -> consult('tests/tmp/m19_init.pl') ; true)), L), findall(Y, m19_i(Y), L2)), solutions([(L = [1, 2, 3], L2 = [1, 2])])).
% 指示が abolish して足し直す。脇に置いた走査が消した節に届くので、述語は abolish 済みの枠で残り、足した節は
% 続きの番号になる。消していたときは、作り直した述語の番号が先頭から振り直され、鍵で選んだ候補の番号が新しい節を
% 指した（第1引数を束縛した走査は [o0, n1, n2] を、束縛しない走査は [o0] を答えた）。
:- dynamic(m19_k/2).
m19_k(1, o0). m19_k(1, o1). m19_k(1, o2). m19_k(1, o3). m19_k(1, o4).
m19_k(1, o5). m19_k(1, o6). m19_k(1, o7). m19_k(1, o8). m19_k(1, o9).
:- dynamic(m19_u/2).
m19_u(1, o0). m19_u(1, o1). m19_u(1, o2). m19_u(1, o3). m19_u(1, o4).
m19_u(1, o5). m19_u(1, o6). m19_u(1, o7). m19_u(1, o8). m19_u(1, o9).
m19_put(S, Directive) :- write(S, ':- '), writeq(S, Directive), write(S, '.\n').
m19_renew(Path, Name) :- open(Path, write, S), m19_put(S, abolish(Name/2)), m19_put(S, dynamic(Name/2)), m19_renew_add(S, Name, [n0, n1, n2]), close(S).
m19_renew_add(_, _, []).
m19_renew_add(S, Name, [V|Vs]) :- Clause =.. [Name, 1, V], m19_put(S, assertz(Clause)), m19_renew_add(S, Name, Vs).
test(m19_parked_keyed_abolish, (m19_renew('tests/tmp/m19_keyed.pl', m19_k), findall(V, (m19_k(1, V), (V == o0 -> consult('tests/tmp/m19_keyed.pl') ; true)), L), findall(W, m19_k(1, W), L2)), solutions([(L = [o0, o1, o2, o3, o4, o5, o6, o7, o8, o9], L2 = [n0, n1, n2])])).
test(m19_parked_cursor_abolish, (m19_renew('tests/tmp/m19_cursor.pl', m19_u), findall(V, (m19_u(_, V), (V == o0 -> consult('tests/tmp/m19_cursor.pl') ; true)), L), findall(W, m19_u(_, W), L2)), solutions([(L = [o0, o1, o2, o3, o4, o5, o6, o7, o8, o9], L2 = [n0, n1, n2])])).
% retract は別のゴールが消した節を消さない。脇に置いた retract の走査も、直接 abolish したときと同じく [1] で終わる。
:- dynamic(m19_t/1).
m19_t(1). m19_t(2). m19_t(3).
test(m19_parked_retract, (m19_renew_t, findall(X, (retract(m19_t(X)), (X =:= 1 -> consult('tests/tmp/m19_retract.pl') ; true)), L), findall(Y, m19_t(Y), L2)), solutions([(L = [1], L2 = [10, 20])])).
m19_renew_t :- m19_write('tests/tmp/m19_retract.pl', ':- abolish(m19_t/1).\n:- dynamic(m19_t/1).\n:- assertz(m19_t(10)).\n:- assertz(m19_t(20)).\n').
% 指示の中の consult が述語を置き換える。外の走査が脇に置かれていても置き換え、古い節は retract と同じく、外の
% 走査がまだ試す間は死んだ節で残す。前は置き換えで消え、外の走査は残りの節を失った（L = [1]）。
m19_r_setup :-
    m19_write('tests/tmp/m19_r_old.pl', ':- dynamic(m19_r/1).\nm19_r(1).\nm19_r(2).\nm19_r(3).\n'),
    m19_write('tests/tmp/m19_r_new.pl', ':- dynamic(m19_r/1).\nm19_r(10).\nm19_r(20).\n'),
    m19_write('tests/tmp/m19_r_dir.pl', ':- consult(\'tests/tmp/m19_r_new.pl\'), true.\n'),
    consult('tests/tmp/m19_r_old.pl').
test(m19_parked_reconsult, (m19_r_setup, findall(X, (m19_r(X), (X =:= 1 -> consult('tests/tmp/m19_r_dir.pl') ; true)), L), findall(Y, m19_r(Y), L2)), solutions([(L = [1, 2, 3], L2 = [10, 20])])).
% abolish と dynamic の後に節を読み直すファイル。abolish の後も外の走査が届く節が残るが、dynamic が印を外すので、
% 節は置き換えの道を通る。置き換えを断っていたときは読み込みが失敗した（M19 の Codex レビュー P2）。
:- dynamic(m19_d/1).
m19_d(1). m19_d(2). m19_d(3).
m19_d_text(':- abolish(m19_d/1).\n:- dynamic(m19_d/1).\nm19_d(10).\nm19_d(20).\n').
test(m19_parked_redefine, (m19_d_text(Text), m19_write('tests/tmp/m19_redefine.pl', Text), findall(X, (m19_d(X), (X =:= 1 -> consult('tests/tmp/m19_redefine.pl') ; true)), L), findall(Y, m19_d(Y), L2)), solutions([(L = [1, 2, 3], L2 = [10, 20])])).
% 開いた走査が生きた節を全部消した述語を読み直す。死んだ節だけの述語は空の述語と同じで、選択点の下でも読み直し、
% 動的のまま残る（M19 の敵対的レビュー B1）。前は選択点があると断り、[2, 3] と [] になった。
:- dynamic(m19_b/1).
m19_b(1). m19_b(2). m19_b(3).
test(m19_dead_only_reload, (m19_write('tests/tmp/m19_dead_only.pl', 'm19_b(10).\n'), findall(X, (m19_b(X), (X =:= 1 -> (retract(m19_b(_)), fail ; true), consult('tests/tmp/m19_dead_only.pl') ; true)), L), assertz(m19_b(99)), findall(Y, m19_b(Y), L2)), solutions([(L = [1, 2, 3], L2 = [10, 99])])).
% multifile の述語は置き換えずに足す。外の走査は後から足した節を見ない（M19 の Codex レビュー P2）。
:- multifile(m19_m/1).
:- dynamic(m19_m/1).
m19_m(1). m19_m(2).
m19_m_setup :-
    m19_write('tests/tmp/m19_m_more.pl', ':- multifile(m19_m/1).\n:- dynamic(m19_m/1).\nm19_m(3).\n'),
    m19_write('tests/tmp/m19_m_dir.pl', ':- consult(\'tests/tmp/m19_m_more.pl\'), true.\n').
test(m19_parked_multifile, (m19_m_setup, findall(X, (m19_m(X), (X =:= 1 -> consult('tests/tmp/m19_m_dir.pl') ; true)), L), findall(Y, m19_m(Y), L2)), solutions([(L = [1, 2], L2 = [1, 2, 3])])).
% 2 段の入れ子。指示の中の consult の指示が消した節も、一番外の走査が試す。
:- dynamic(m19_n/1).
m19_n(1). m19_n(2). m19_n(3).
m19_n_setup :-
    m19_write('tests/tmp/m19_nested_1.pl', ':- consult(\'tests/tmp/m19_nested_2.pl\'), true.\n'),
    m19_write('tests/tmp/m19_nested_2.pl', ':- retract(m19_n(3)).\n').
test(m19_parked_nested, (m19_n_setup, findall(X, (m19_n(X), (X =:= 1 -> consult('tests/tmp/m19_nested_1.pl') ; true)), L), findall(Y, m19_n(Y), L2)), solutions([(L = [1, 2, 3], L2 = [1, 2])])).
% 脇に置いた選択点は置き換えを断らない。once/1 の中で読む初期化のゴールは述語を置き換える（M19 の敵対的レビュー
% A1）。脇に置いた選択点を全部数えていたときは黙って断り、古い定義が残った。
m19_o_setup :-
    m19_write('tests/tmp/m19_o_old.pl', 'm19_o(1).\n'),
    m19_write('tests/tmp/m19_o_new.pl', 'm19_o(2).\n'),
    m19_write('tests/tmp/m19_o_init.pl', ':- initialization(consult(\'tests/tmp/m19_o_new.pl\')).\n'),
    consult('tests/tmp/m19_o_old.pl').
test(m19_parked_once, (m19_o_setup, once(consult('tests/tmp/m19_o_init.pl')), findall(X, m19_o(X), L)), solutions([L = [2]])).

% 開いた走査が見ている先の節を retract が消す。走査は呼んだ時点の節を見るが、retract は消えた節を二度は消さない。
test(m3_sweep_held, (m3_sh(X, _), retract(m3_sh(Y, _))), solutions([(X = 1, Y = 1), (X = 1, Y = 2), (X = 1, Y = 3), (X = 1, Y = 4), (X = 1, Y = 5), (X = 1, Y = 6), (X = 1, Y = 7), (X = 1, Y = 8), (X = 1, Y = 9), (X = 1, Y = 10)])).
test(m3_sweep_held_left, (findall(X, m3_sh(X, _), L), predicate_property(m3_sh(_, _), number_of_clauses(N))), solutions([(L = [], N = 0)])).
% 真ん中から消した節は掃除まで節の列に残るが、節の数は生きた節だけを数える。
test(m3_retract_counts, (retract(m3_rh(2)), retract(m3_rh(4)), retract(m3_rh(6)), retract(m3_rh(8)), retract(m3_rh(10)), predicate_property(m3_rh(_), number_of_clauses(N)), findall(X, m3_rh(X), L)), solutions([(N = 5, L = [1, 3, 5, 7, 9])])).
% 真ん中の 9 節を消すと半分を超え、次の assertz の前に掃除する。掃除の後も鍵で引け、消した鍵を足し直すと末尾に入る。
test(m3_retract_reassert, (m3_rr_drop(2, 10), assertz(m3_rr(17)), findall(X, m3_rr(X), L1), findall(y, m3_rr(12), In), findall(y, m3_rr(5), Out), assertz(m3_rr(5)), findall(B, clause(m3_rr(5), B), L2), findall(X, m3_rr(X), L3)), solutions([(L1 = [1, 11, 12, 13, 14, 15, 16, 17], In = [y], Out = [], L2 = [true], L3 = [1, 11, 12, 13, 14, 15, 16, 17, 5])])).
% 開いた走査の先の節を半分を超えて消すと、走査の中の assertz の前に掃除する。走査がまだ試す節は掃除で残し、
% 走査は呼んだ時点の節を全部見る。カーソルの呼び出し、clause/2 のカーソル、鍵で引いた候補の列の 3 通り。
test(m3_sweep_open, (m3_so(X), (X == 1 -> retract(m3_so(2)), retract(m3_so(3)), retract(m3_so(4)), assertz(m3_so(6)) ; true)), solutions([X = 1, X = 2, X = 3, X = 4, X = 5])).
test(m3_sweep_open_left, findall(X, m3_so(X), L), solutions([L = [1, 5, 6]])).
test(m3_sweep_open_clause, findall(X, (clause(m3_sq(X), true), (X == 1 -> retract(m3_sq(2)), retract(m3_sq(3)), retract(m3_sq(4)), assertz(m3_sq(6)) ; true)), L), solutions([L = [1, 2, 3, 4, 5]])).
test(m3_sweep_open_clause_left, findall(X, m3_sq(X), L), solutions([L = [1, 5, 6]])).
test(m3_sweep_open_list, (m3_sl(k, X), (X == 1 -> m3_sl_drop(2, 7), assertz(m3_sl(k, 10)) ; true)), solutions([X = 1, X = 2, X = 3, X = 4, X = 5, X = 6, X = 7, X = 8, X = 9])).
test(m3_sweep_open_list_left, findall(X, m3_sl(k, X), L), solutions([L = [1, 8, 9, 10]])).
% カットで閉じた走査は節を持たない。全部消すと次の呼び出しは何も見ず、足した節だけを見る。
test(m3_cut_then_retract, ((m3_ct(X, _), ! ; true), (retract(m3_ct(_, _)), fail ; true), findall(Y, m3_ct(Y, _), L), assertz(m3_ct(z, b)), findall(Y, m3_ct(Y, _), L2)), solutions([(X = 1, L = [], L2 = [z])])).
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
