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
test(iso_float_fun, (X is float(1)), solutions([X = 1.0])).
test(iso_eq_sum, (1 + 2 =:= 3), true).
test(iso_le, (1 =< 1), true).
test(iso_gt, (2 > 1), true).
test(iso_ne, (1 =\= 2), true).
test(iso_msb_zero, (X is msb(0)), error(evaluation_error(undefined))).
test(iso_pow, (X is 2 ** 3), solutions([X = 8])).
