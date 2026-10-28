test(quot, (X is -5 // 2), solutions([X = -2])).
test(floor_div, (X is div(-5, 2)), solutions([X = -3])).
test(zero, (X is 1 / 0), error(evaluation_error(zero_divisor))).
test(overflow, (X is 9223372036854775807 + 1), error(evaluation_error(int_overflow))).
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
test(nan_eq, (nan =:= nan), fail).
test(nan_ne, (nan =\= nan), true).
test(nan_lt, (nan < 1), fail).
test(nan_le, (nan =< 1), fail).
test(nan_gt, (1 > nan), fail).
test(nan_ge, (1 >= nan), fail).
test(abs_min, (X is abs(-9223372036854775807 - 1)), error(evaluation_error(int_overflow))).
test(neg_min, (X is -(-9223372036854775807 - 1)), error(evaluation_error(int_overflow))).
test(mod_edge, (X is (-9223372036854775807 - 1) mod 9223372036854775807), solutions([X = 9223372036854775806])).
test(int_pow63, (X is integer(2.0 ** 63)), error(evaluation_error(int_overflow))).
test(floor_pow63, (X is floor(2.0 ** 63)), error(evaluation_error(int_overflow))).
test(trunc_pow63, (X is truncate(2.0 ** 63)), error(evaluation_error(int_overflow))).
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
test(hat_neg, (X is ^(2, -1)), error(evaluation_error(undefined))).
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
