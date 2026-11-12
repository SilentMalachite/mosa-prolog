% レビュー L20 のテストが使う式の組み立て。test/3 の節を離さないよう先頭に置く。
l20_right(0, X, X) :- !.
l20_right(N, X, Y) :- M is N - 1, l20_right(M, 1+X, Y).
l20_left(0, X, X) :- !.
l20_left(N, X, Y) :- M is N - 1, l20_left(M, X+1, Y).
% 葉ごとに同じ部分項 S を共有する深い式。共有は循環ではない。
l20_shared(0, _, X, X) :- !.
l20_shared(N, S, X, Y) :- M is N - 1, l20_shared(M, S, X+S, Y).

test(quot, (X is -5 // 2), solutions([X = -2])).
test(floor_div, (X is div(-5, 2)), solutions([X = -3])).
test(zero, (X is 1 / 0), error(evaluation_error(zero_divisor))).
test(overflow, (X is 9223372036854775807 + 1, X == 9223372036854775808), true).
test(numeric_eq, (1.0 =:= 1), true).
test(numeric_ne, (1.0 =\= 2), true).
test(numeric_lt, (1 < 2.0), true).
test(abs_neg, (X is abs(-3)), solutions([X = 3])).
test(sign_neg, (X is sign(-3)), solutions([X = -1])).
test(max_n, (X is max(1, 2)), solutions([X = 2])).
test(gcd_n, (X is gcd(12, 8)), solutions([X = 4])).
test(msb1, (X is msb(1)), solutions([X = 0])).
test(msb2, (X is msb(2)), solutions([X = 1])).
test(succ0, (X is succ_or_zero(3)), solutions([X = 4])).
test(succ_neg, (X is succ_or_zero(-2)), solutions([X = 0])).
test(sqrt4, (X is sqrt(4)), solutions([X = 2.0])).
test(sqrt_neg, (X is sqrt(-1)), error(evaluation_error(undefined))).
test(pi_pos, (pi > 3), true).
test(float_div, (X is 1 / 2), solutions([X = 0.5])).
test(iso_times, (X is 3 * 4), solutions([X = 12])).
test(iso_div_pos, (X is 7 // 2), solutions([X = 3])).
test(iso_rem, (X is 7 rem 2), solutions([X = 1])).
test(iso_mod, (X is 7 mod 2), solutions([X = 1])).
test(iso_neg_div, (X is -7 // 2), solutions([X = -3])).
test(iso_floor_neg, (X is floor(-1.1)), solutions([X = -2])).
test(iso_ceiling, (X is ceiling(1.1)), solutions([X = 2])).
test(iso_truncate, (X is truncate(1.9)), solutions([X = 1])).
test(iso_round, (X is round(1.5)), solutions([X = 2])).
test(round_neg_half, (X is round(-2.5)), solutions([X = -2])).
test(round_neg_half_odd, (X is round(-3.5)), solutions([X = -3])).
test(round_neg_point5, (X is round(-0.5)), solutions([X = 0])).
test(round_neg_below_half, (X is round(-2.6)), solutions([X = -3])).
test(round_just_below_half, (X is round(0.49999999999999994)), solutions([X = 0])).
test(round_odd_2p52, (X is round(4503599627370497.0)), solutions([X = 4503599627370497])).
test(floor_int, (X is floor(9)), error(type_error(float, 9))).
test(ceiling_int, (X is ceiling(9)), error(type_error(float, 9))).
test(truncate_int, (X is truncate(9)), error(type_error(float, 9))).
test(round_int, (X is round(9)), error(type_error(float, 9))).
test(fip_int, (X is float_integer_part(9)), error(type_error(float, 9))).
test(ffp_int, (X is float_fractional_part(9)), error(type_error(float, 9))).
test(floor_int_expr, (X is floor(4 + 5)), error(type_error(float, 9))).
test(floor_float_expr, (X is floor(4 + 5.5)), solutions([X = 9])).
test(fip_neg_float, (X is float_integer_part(-2.5)), solutions([X = -2.0])).
test(iso_float_fun, (X is float(1)), solutions([X = 1.0])).
test(iso_eq_sum, (1 + 2 =:= 3), true).
test(iso_le, (1 =< 1), true).
test(iso_gt, (2 > 1), true).
test(iso_ne, (1 =\= 2), true).
test(iso_msb_zero, (X is msb(0)), error(evaluation_error(undefined))).
test(iso_pow, (X is 2 ** 3), solutions([X = 8.0])).
% iso-notes 9.3.1.3 d の決定 A。0 の負の冪は undefined。
test(pow_zero_neg_int, (X is 0 ** -1), error(evaluation_error(undefined))).
test(pow_zero_neg_float, (X is 0.0 ** -1.0), error(evaluation_error(undefined))).
test(pow_negzero_neg, (X is -0.0 ** -2), error(evaluation_error(undefined))).
test(pow_zero_neg_frac, (X is 0 ** -0.5), error(evaluation_error(undefined))).
test(hat_zero_neg_float, (X is 0 ^ -1.0), error(evaluation_error(undefined))).
test(pow_zero_zero, (X is 0 ** 0), solutions([X = 1.0])).
test(pow_zero_pos, (X is 0.0 ** 2), solutions([X = 0.0])).
test(nan_eq, (nan =:= nan), fail).
test(nan_ne, (nan =\= nan), true).
test(nan_lt, (nan < 1), fail).
test(nan_le, (nan =< 1), fail).
test(nan_gt, (1 > nan), fail).
test(nan_ge, (1 >= nan), fail).
test(abs_min, (X is abs(-9223372036854775807 - 1), X == 9223372036854775808), true).
test(neg_min, (X is -(-9223372036854775807 - 1), X == 9223372036854775808), true).
test(mod_edge, (X is (-9223372036854775807 - 1) mod 9223372036854775807), solutions([X = 9223372036854775806])).
test(int_pow63, (X is integer(2.0 ** 63), X == 9223372036854775808), true).
test(floor_pow63, (X is floor(2.0 ** 63), X == 9223372036854775808), true).
test(trunc_pow63, (X is truncate(2.0 ** 63), X == 9223372036854775808), true).
test(shl_neg1, (X is -1 << 1), solutions([X = -2])).
test(shl_neg0, (X is -1 << 0), solutions([X = -1])).
test(shl_neg63, (X is -1 << 63, X =:= -9223372036854775807 - 1), true).
test(shl_zero63, (X is 0 << 63), solutions([X = 0])).
test(shr_one63, (X is 1 >> 63), solutions([X = 0])).
test(shr_big, (X is 8 >> 100), solutions([X = 0])).
test(shr_neg63, (X is -1 >> 63), solutions([X = -1])).
test(shl_overflow, (X is 1 << 63), error(evaluation_error(int_overflow))).
test(neg_pow_neg, (X is (-2) ** (-1)), solutions([X = -0.5])).
test(neg_pow_half, (X is (-2) ** 0.5), error(evaluation_error(undefined))).
test(sign_float, (X is sign(-2.5)), solutions([X = -1.0])).
test(int_hat, (X is ^(2, 3)), solutions([X = 8])).
test(unary_plus, (X is +1), solutions([X = 1])).
% iso-notes 9.3.10.3 の決定 B。底が 0、1、-1 以外の整数で指数が負の整数なら type_error(float, 底)。
test(hat_neg, (X is ^(2, -1)), error(type_error(float, 2))).
test(hat_neg2, (X is ^(2, -2)), error(type_error(float, 2))).
test(hat_neg_base, (X is ^(-3, -1)), error(type_error(float, -3))).
test(hat_minus_one_odd, (X is ^(-1, -3)), solutions([X = -1])).
test(hat_minus_one_even, (X is ^(-1, -2)), solutions([X = 1])).
test(hat_zero_neg1, (X is ^(0, -1)), error(evaluation_error(undefined))).
test(hat_zero_neg2, (X is ^(0, -2)), error(evaluation_error(undefined))).
test(hat_one, (X is ^(1, -1)), solutions([X = 1])).
test(atan_zero, (X is atan2(0, 0)), error(evaluation_error(undefined))).
test(div_type, (X is 1.5 // 2), error(type_error(integer, 1.5))).
test(catch_zero, catch((X is 1 / 0), error(evaluation_error(zero_divisor), _), true), true).
test(catch_ne, catch((X =\= 1), error(instantiation_error, _), true), true).
% log/2。log(B, X) は底 B の対数（SPEC 第7節の段階 B）。
test(log2_base, (X is log(2, 8), abs(X - 3.0) < 1.0e-12), true).
test(log2_same, (X is log(10, 10), abs(X - 1.0) < 1.0e-12), true).
test(log2_int_result_float, (X is log(2, 4), float(X)), true).
test(log2_zero_base, (X is log(0, 10)), error(evaluation_error(undefined))).
test(log2_zero_x, (X is log(10, 0)), error(evaluation_error(undefined))).
test(log2_neg_base, (X is log(-1, 10)), error(evaluation_error(undefined))).
test(log2_neg_x, (X is log(10, -1)), error(evaluation_error(undefined))).
test(log2_base_one, (X is log(1, 10)), error(evaluation_error(undefined))).
test(log2_inst, (X is log(_, 10)), error(instantiation_error)).
% 7.9.1。主関数子を引数より先に確かめる（iso-notes 7.9.1 の決定 B）。
test(eval_functor_first, (X is foo(bar)), error(type_error(evaluable, foo/1))).
test(eval_functor_before_var, (X is foo(_)), error(type_error(evaluable, foo/1))).
test(eval_functor_nested, (X is 1 + foo(bar)), error(type_error(evaluable, foo/1))).
test(eval_functor_cmp, (foo(bar) =:= 1), error(type_error(evaluable, foo/1))).
test(eval_functor_arity, (X is sqrt(bar, 1)), error(type_error(evaluable, sqrt/2))).
test(eval_arg_atom, (X is 1 + bar), error(type_error(evaluable, bar/0))).
test(eval_known_functor_arg, (X is log(foo, 2)), error(type_error(evaluable, foo/0))).

% レビュー L1。最小の整数を -1 で割った余りは 0 で、桁はあふれない（9.1.3）。商は int64 に入らない 2^63 になる（フェーズ10）。
test(l1_rem_min, (M is -9223372036854775807 - 1, X is M rem -1), solutions([X = 0])).
test(l1_mod_min, (M is -9223372036854775807 - 1, X is M mod -1), solutions([X = 0])).
test(l1_quot_min, (M is -9223372036854775807 - 1, X is M // -1, X == 9223372036854775808), true).
test(l1_div_min, (M is -9223372036854775807 - 1, X is div(M, -1), X == 9223372036854775808), true).
% gcd は最小の整数を含んでも、値が収まれば返す。2^63 になるときだけあふれる。
test(l1_gcd_min_2, (M is -9223372036854775807 - 1, X is gcd(M, 2)), solutions([X = 2])).
test(l1_gcd_2_min, (M is -9223372036854775807 - 1, X is gcd(-6, M)), solutions([X = 2])).
test(l1_gcd_min_half, (M is -9223372036854775807 - 1, X is gcd(M, 4611686018427387904)), solutions([X = 4611686018427387904])).
test(l1_gcd_min_0, (M is -9223372036854775807 - 1, X is gcd(M, 0)), error(evaluation_error(int_overflow))).
test(l1_gcd_min_min, (M is -9223372036854775807 - 1, X is gcd(M, M)), error(evaluation_error(int_overflow))).
% max と min は、型の違う引数を float に直して比べ、float を返す（iso-notes 9.3.8 の決定 B）。値の型が値で変わらないので、
% 丸めの関数にそのまま渡せる（9.1.6 の決定 A で floor は整数を受け付けない）。2^53 を超える整数は丸まる。
test(l1_max_mixed_float, (X is max(2.0, 3)), solutions([X = 3.0])).
test(l1_min_mixed_float, (X is min(2, 3.0)), solutions([X = 2.0])).
test(l1_max_float_arg, (X is max(2, 3.0)), solutions([X = 3.0])).
test(l1_max_tie, (X is max(0, 0.0)), solutions([X = 0.0])).
test(l1_min_tie, (X is min(0, 0.0)), solutions([X = 0.0])).
test(l1_max_half, (X is max(2, 2.5)), solutions([X = 2.5])).
test(l1_min_half, (X is min(-2, -2.5)), solutions([X = -2.5])).
test(l1_max_rounded, (X is max(9007199254740993, 9007199254740992.0)), solutions([X = 9007199254740992.0])).
test(l1_floor_of_max, (X is floor(max(0.0, 3))), solutions([X = 3])).
test(l1_max_inf, (X is max(3, inf), Y is inf, X == Y), true).
% NaN との比べは大小どちらも偽なので、max は左、min は右の引数を返す。
test(l1_max_nan_right, (X is max(1, nan), X == 1.0), true).
test(l1_min_nan_left, (X is min(nan, 1), X == 1.0), true).
% レビュー L2（iso-notes 9.1.4 の決定 A）。sign/1 は NaN をそのまま返す。NaN だけが自分と =:= にならない。
test(l2_sign_nan, (X is sign(nan), \+ X =:= X), true).
test(l2_sign_neg_nan, (X is sign(-(nan)), \+ X =:= X), true).
test(l2_sign_neg_zero, (X is sign(-0.0), X == -0.0), true).
test(l2_sign_zero, (X is sign(0.0), X == 0.0), true).
% レビュー L2（iso-notes 9.1.4 の決定 A）。integer/1（SWI の拡張）は近い整数へ丸め、ちょうど半分は 0 から遠い方へ。
% round/1 は 9.1.6.1 の floor(x + 1/2) なので、負の半分だけが違う。
test(l2_integer_half, (X is integer(2.5)), solutions([X = 3])).
test(l2_integer_neg_half, (X is integer(-2.5)), solutions([X = -3])).
test(l2_integer_below_half, (X is integer(2.4)), solutions([X = 2])).
test(l2_integer_neg_above_half, (X is integer(-2.6)), solutions([X = -3])).
test(l2_integer_small_half, (X is integer(0.5), Y is integer(-0.5)), solutions([(X = 1, Y = -1)])).
test(l2_integer_int, (X is integer(7)), solutions([X = 7])).
% 整数は double を通さない（L2 の敵対的レビュー A2）。2^53 + 1 と max_integer も変わらない。
test(l2_integer_int_exact, (X is integer(9007199254740993)), solutions([X = 9007199254740993])).
test(l2_integer_int_max, (X is integer(9223372036854775807)), solutions([X = 9223372036854775807])).
test(l2_integer_int_min, (M is -9223372036854775807 - 1, X is integer(M), X =:= M), true).
test(l2_integer_overflow, (X is integer(1.0e300), X == 1000000000000000052504760255204420248704468581108159154915854115511802457988908195786371375080447864043704443832883878176942523235360430575644792184786706982848387200926575803737830233794788090059368953234970799945081119038967640880074652742780142494579258788820056842838115669472196386865459400540160), true).
test(l2_integer_nan, (X is integer(nan)), error(evaluation_error(undefined))).
test(l2_round_neg_half, (X is round(-2.5)), solutions([X = -2])).
% L2 の敵対的レビュー A3（iso-notes 9.3.8 の追記）。-0.0 は 0.0 と 0 より小さいとみなし、引数の順によらない。
test(l2_max_zero_left, (X is max(-0.0, 0.0), X == 0.0), true).
test(l2_max_zero_right, (X is max(0.0, -0.0), X == 0.0), true).
test(l2_min_zero_left, (X is min(-0.0, 0.0), X == -0.0), true).
test(l2_min_zero_right, (X is min(0.0, -0.0), X == -0.0), true).
test(l2_max_zero_int, (X is max(-0.0, 0), Y is max(0, -0.0), X == 0.0, Y == 0.0), true).
test(l2_min_zero_int, (X is min(-0.0, 0), Y is min(0, -0.0), X == -0.0, Y == -0.0), true).
test(l2_max_zero_same, (X is max(-0.0, -0.0), X == -0.0), true).

% レビュー L20 のテスト（持ち主の決定 A、iso-notes 7.3.3）。循環する式は resource_error(cyclic_term) を投げ、止まる。
% 評価する前に誤りになるもの（評価できない関数子、未束縛の変数）は、左から順に評価して先に出会えばそちらになる。
test(l20_cyclic_left, (X = X+1, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_right, (X = 1+X, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_both, (X = X*X, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_unary, (X = -X, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_mutual, (X = Y+1, Y = X*2, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_inner, (X = 1+X, _ is 2*(3-X)), error(resource_error(cyclic_term))).
test(l20_cyclic_long_period, (l20_right(300, X, E), X = E, _ is E), error(resource_error(cyclic_term))).
test(l20_cyclic_after_deep, (l20_right(300, X, E), X = 1+X, _ is E), error(resource_error(cyclic_term))).
test(l20_cyclic_side_first, (l20_left(500, 0, S), X = S+X, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_side_both, (l20_left(500, 0, S), X = (S+X)+S, _ is X), error(resource_error(cyclic_term))).
% 閉路の中に深い脇道がある。見張りは脇道へ移っても、脇道を評価し終えれば親へ下がるので、手間は脇道の深さに比例する
% （敵対的レビュー A1。深さが倍になるたびに真ん中の段を探した前の版は 2 乗で、深さ 32000 に 48 秒かかった）。
test(l20_cyclic_side_deep, (l20_left(20000, 0, S), X = S+X, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_side_deep_prefix, (l20_left(1000, 0, S), X = S+X, l20_right(20000, X, E), _ is E),
     error(resource_error(cyclic_term))).
% 閉路の前の部分が 33〜63 段。C++ の再帰の見張りを深さだけで移した版は、ここで閉路を 31 周まで回った（再確認の B1）。
test(l20_cyclic_side_mid, (l20_left(2000, 0, S), X = S+X, l20_right(40, X, E), _ is E), error(resource_error(cyclic_term))).
test(l20_cyclic_var_later, (X = X+_, _ is X), error(resource_error(cyclic_term))).
test(l20_cyclic_var_first, (X = _+X, _ is X), error(instantiation_error)).
test(l20_cyclic_evaluable_first, (X = foo+X, _ is X), error(type_error(evaluable, foo/0))).
test(l20_cyclic_not_evaluable, (X = f(X), _ is X), error(type_error(evaluable, f/1))).
test(l20_cyclic_compare_left, (X = X+1, X > 0), error(resource_error(cyclic_term))).
test(l20_cyclic_compare_right, (X = 1+X, 0 =:= X), error(resource_error(cyclic_term))).
test(l20_cyclic_compare_ne, (X = 1+X, X =\= 0), error(resource_error(cyclic_term))).
test(l20_cyclic_caught, (X = X+1, catch(_ is X, error(E, _), true), Y is 1+2), solutions([(E = resource_error(cyclic_term), Y = 3)])).
test(l20_deep_not_cyclic, (l20_right(2000, 0, E), X is E), solutions([X = 2000])).
% 見張りが共有の S へ移っても、S を評価し終えれば親へ下がる。下がらなければ次の S で同じ項に出会い、循環と取り違える
% （CycleWatch::returned を外すとこれが落ちる）。
test(l20_shared_not_cyclic, (S = 2*3, l20_shared(2000, S, 0, E), X is E), solutions([X = 12000])).
% 深い部分項 T を 2 つの引数で共有する。
test(l20_shared_deep_twice, (l20_right(2000, 0, T), X is T + T), solutions([X = 4000])).
test(l20_shared_deep_aliases, (l20_right(2000, 0, T), A = T, B = T, X is A - B), solutions([X = 0])).
