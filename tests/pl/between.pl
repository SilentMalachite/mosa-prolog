use_between :- use_module(library(between)).

% 取り込むまで user からは見えない。
test(between_hidden, between(1, 2, 1), error(existence_error(procedure, between/3))).

% Low から High まで、両端を含めて整数を順に出す。
test(between_gen, (use_between, findall(N, between(1, 3, N), [1, 2, 3])), true).
test(between_low_atom, (use_between, between(a, 3, _)), error(type_error(integer, a))).
test(between_high_atom, (use_between, between(1, a, _)), error(type_error(integer, a))).
test(between_value_atom, (use_between, between(1, 3, a)), error(type_error(integer, a))).
test(between_low_var, (use_between, between(_, 3, _)), error(instantiation_error)).
test(between_high_var, (use_between, between(1, _, _)), error(instantiation_error)).
test(between_ends, (use_between, between(1, 5, 1), between(1, 5, 5)), true).
test(between_mid, (use_between, between(1, 5, 3)), true).
test(between_low_miss, (use_between, between(1, 3, 0)), fail).
test(between_high_miss, (use_between, between(1, 3, 5)), fail).
test(between_empty, (use_between, between(1, 0, _)), fail).
test(between_one, (use_between, findall(N, between(1, 1, N), [1])), true).
test(between_neg, (use_between, findall(N, between(-1, 1, N), [-1, 0, 1])), true).
test(between_float, (use_between, between(1.5, 3, _)), error(type_error(integer, 1.5))).
% 範囲が空でも、値の型を先に見る。
test(between_type_before_range, (use_between, between(1, 0, a)), error(type_error(integer, a))).
% Low の型は、High が変数でも instantiation より先。
test(between_low_before_high, (use_between, between(a, _, _)), error(type_error(integer, a))).
test(succ_next, (use_between, succ(1, N), N = 2), true).
test(succ_prev, (use_between, succ(N, 2), N = 1), true).
test(succ_zero, (use_between, succ(_, 0)), fail).
test(succ_neg, (use_between, succ(-1, _)), error(domain_error(not_less_than_zero, -1))).
test(succ_neg_succ, (use_between, succ(_, -1)), error(domain_error(not_less_than_zero, -1))).
test(succ_type_a, (use_between, succ(a, _)), error(type_error(integer, a))).
test(succ_type_b, (use_between, succ(_, a)), error(type_error(integer, a))).
test(succ_type_ground, (use_between, succ(1, a)), error(type_error(integer, a))).
test(succ_neg_ground, (use_between, succ(1, -1)), error(domain_error(not_less_than_zero, -1))).
% 負より、整数でない方を先に返す。
test(succ_type_before_domain, (use_between, succ(-1, a)), error(type_error(integer, a))).
test(succ_open, (use_between, succ(_, _)), error(instantiation_error)).
test(succ_mismatch, (use_between, succ(3, 2)), fail).
test(succ_pair, (use_between, succ(1, 2)), true).
test(succ_from_zero, (use_between, succ(0, 1)), true).
test(succ_overflow, (use_between, current_prolog_flag(max_integer, Max), succ(Max, _)), error(evaluation_error(int_overflow))).
test(plus_sum, (use_between, plus(1, 2, N), N = 3), true).
test(plus_right, (use_between, plus(1, N, 3), N = 2), true).
test(plus_left, (use_between, plus(N, 2, 3), N = 1), true).
test(plus_type_a, (use_between, plus(a, 1, _)), error(type_error(integer, a))).
test(plus_type_b, (use_between, plus(1, a, _)), error(type_error(integer, a))).
test(plus_type_c, (use_between, plus(_, 1, a)), error(type_error(integer, a))).
test(plus_type_b_sum, (use_between, plus(_, a, 1)), error(type_error(integer, a))).
test(plus_type_c_left, (use_between, plus(1, _, a)), error(type_error(integer, a))).
test(plus_type_sum, (use_between, plus(1, 2, a)), error(type_error(integer, a))).
test(plus_mismatch, (use_between, plus(1, 2, 4)), fail).
test(plus_ground, (use_between, plus(1, 2, 3)), true).
test(plus_neg, (use_between, plus(-1, 2, N), N = 1), true).
test(plus_open, (use_between, plus(_, _, _)), error(instantiation_error)).
test(plus_one_a, (use_between, plus(1, _, _)), error(instantiation_error)).
test(plus_one_b, (use_between, plus(_, 1, _)), error(instantiation_error)).
test(plus_one_c, (use_between, plus(_, _, 1)), error(instantiation_error)).
test(plus_float, (use_between, plus(1.5, 1, _)), error(type_error(integer, 1.5))).
test(plus_overflow, (use_between, current_prolog_flag(max_integer, Max), plus(Max, 1, _)), error(evaluation_error(int_overflow))).
