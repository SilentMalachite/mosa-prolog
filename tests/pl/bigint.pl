% フェーズ10（ADR 0025）。int64 に入らない整数（Big）のテスト。字句解析、reader、number_codes/2 の族が字面から
% Big を作り、算術（タスク 6）が Big を計算する。値は == で比べ、int64 に収まる結果が Big にならないことも見る。
% ^ の Big はタスク 7 なので、10 と 2 の冪は * だけで作る。
pow10(0, 1) :- !.
pow10(N, X) :- M is N - 1, pow10(M, Y), X is Y * 10.
pow2(0, 1) :- !.
pow2(N, X) :- M is N - 1, pow2(M, Y), X is Y * 2.
sum_expr(0, _, 0) :- !.
sum_expr(N, P, E + P) :- M is N - 1, sum_expr(M, P, E).
test(lit_big, (X = 123456789012345678901234567890, Y = 123456789012345678901234567890, X == Y), true).
test(lit_distinct, (X = 123456789012345678901234567890, Y = 123456789012345678901234567891, X \== Y), true).
test(lit_hex, (X = 0xffffffffffffffffffff, X == 1208925819614629174706175), true).
test(lit_sep, (X = 1_000_000_000_000_000_000_000, X == 1000000000000000000000), true).
% -2^63 は int64 に入るので、Big ではなく min_integer と同じ IntBox になる（ADR 0025 の正規形）。
test(neg_min, (X = -9223372036854775808, current_prolog_flag(min_integer, M), X == M), true).
test(nc_big, (number_codes(X, "-1267650600228229401496703205376"), number_codes(X, C), atom_codes(A, C), A == '-1267650600228229401496703205376'), true).
% 負の Big を演算子の引数に書いた字面は、読み戻すと元の項になる（7.10.5）。
test(wq_neg_1, (X = -(-1267650600228229401496703205376), term_to_atom(X, A), term_to_atom(Y, A), Y == X), true).
test(wq_neg_2, (X = 1 - (-1267650600228229401496703205376), term_to_atom(X, A), term_to_atom(Y, A), Y == X), true).
test(wq_neg_3, (X = -(1267650600228229401496703205376), Y = -1267650600228229401496703205376, term_to_atom(X, A), term_to_atom(Y, B), A \== B, term_to_atom(X1, A), X1 == X, term_to_atom(Y1, B), Y1 == Y), true).
test(atom_number_big, (atom_number('18446744073709551616', N), N == 18446744073709551616), true).
% 算術（タスク 6）。int64 を越えた結果は Big になり、int64 に戻った結果は Int か IntBox に戻る（正規形）。
test(norm_add, (X is 9223372036854775807 + 1 - 1, X == 9223372036854775807), true).
test(norm_mul, (X is 9223372036854775807 * 2, X == 18446744073709551614), true).
test(norm_min, (X is -9223372036854775808 - 1, X == -9223372036854775809), true).
test(norm_neg, (X is -(-9223372036854775808), X == 9223372036854775808), true).
test(norm_abs, (X is abs(-9223372036854775808), X == 9223372036854775808), true).
test(norm_quot, (X is -9223372036854775808 // -1, X == 9223372036854775808), true).
test(norm_sub_big, (X is 18446744073709551616 - 18446744073709551615, X == 1), true).
test(norm_mul_min, (X is -9223372036854775808 * -9223372036854775808, X == 85070591730234615865843651857942052864), true).
% P = 2^100。// と rem は 0 の側へ、div と mod は負の無限大の側へ丸める（9.1.3）。
test(big_div_quot, (X is -1267650600228229401496703205376 // 7, X == -181092942889747057356671886482), true).
test(big_div_rem, (X is -1267650600228229401496703205376 rem 7, X == -2), true).
test(big_div_div, (X is div(-1267650600228229401496703205376, 7), X == -181092942889747057356671886483), true).
test(big_div_mod, (X is -1267650600228229401496703205376 mod 7, X == 5), true).
test(big_div_zero, (X is 1267650600228229401496703205376 // 0), error(evaluation_error(zero_divisor))).
test(big_cmp_gt, 1267650600228229401496703205376 > 9223372036854775807, true).
test(big_cmp_lt, -1267650600228229401496703205376 < -9223372036854775808, true).
test(big_cmp_eq, 1267650600228229401496703205376 =:= 1267650600228229401496703205376, true).
test(big_cmp_ne, 1267650600228229401496703205376 =\= 1267650600228229401496703205376 + 1, true).
test(big_minmax_max, (X is max(1267650600228229401496703205376, 24), X == 1267650600228229401496703205376), true).
test(big_minmax_min, (X is min(-1267650600228229401496703205376, 0), X == -1267650600228229401496703205376), true).
test(big_sign, (X is sign(-1267650600228229401496703205376), X == -1), true).
% Big を double にするときは最近接偶数に丸める。2^65 + 2^12 はちょうど中間なので偶数の 2^65 へ、1 越えれば上へ。
test(big_float, (X is float(1267650600228229401496703205376), X == 1.2676506002282294e30), true).
test(big_float_tie, (X is float(36893488147419107328), X == 3.6893488147419103e19), true).
test(big_float_above_tie, (X is float(36893488147419107329), X == 3.689348814741911e19), true).
test(big_float_mixed, (X is 1267650600228229401496703205376 + 0.5, X == 1.2676506002282294e30), true).
test(big_float_min, (X is min(1267650600228229401496703205376, 1.0), X == 1.0), true).
test(big_float_cmp, 1267650600228229401496703205376 > 1.0e30, true).
% 整数どうしの / は、被演算子を double にせず、商を 1 回だけ丸める。両方を double にすると 6.148914691236517e18 になる。
test(big_float_div, (pow10(400, A), pow10(399, B), X is A / B, X == 10.0), true).
test(big_float_div_round, (X is 18446744073709552128 / 3, X == 6.148914691236518e18), true).
% 非正規化数の位で丸める。1 / 2^1075 はちょうど 2^-1075 なので偶数の 0.0 へ、それより大きい商は 2^-1074 へ。
test(big_float_div_subnormal, (pow2(1073, B), C is 3 * B, X is 1 / C, X == 5.0e-324), true).
test(big_float_div_tie_zero, (pow2(1075, B), X is 1 / B, X == 0.0), true).
test(big_float_div_above_tie, (pow2(1075, B), C is B - 1, X is 1 / C, X == 5.0e-324), true).
% double の範囲を越える Big は float にできない（iso-notes「8.7, 9.3.8 and 9.3.9」の既定 A）。
test(big_float_over, (pow10(400, A), X is float(A)), error(evaluation_error(float_overflow))).
test(big_float_over_cmp, (pow10(400, A), A > 1.0), error(evaluation_error(float_overflow))).
test(big_round, (X is round(1.0e32), X == 100000000000000005366162204393472), true).
test(big_truncate, (X is truncate(-1.0e20), X == -100000000000000000000), true).
% 引数の Big は親の値を求めたら放す（settle_bigs）。親の値が引数そのもの（max）でも正しく残る。
test(big_settle_arg, (X is max(1267650600228229401496703205376, 1) * 1 + max(-1, 1267650600228229401496703205376), X == 2535301200456458802993406410752), true).
% 64 段より深い式は手で積んで評価する（eval_deep）。Big の葉が 100 個あっても値は正確。
test(big_deep_sum, (sum_expr(100, 1267650600228229401496703205376, E), X is E, X == 126765060022822940149670320537600), true).
test(big_msb, (X is msb(1267650600228229401496703205376), X == 100), true).
test(bounded_flag, current_prolog_flag(bounded, false), true).
test(bounded_max_integer, current_prolog_flag(max_integer, 9223372036854775807), true).
