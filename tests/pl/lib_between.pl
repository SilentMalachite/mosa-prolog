:- use_module(library(between)).
% between/3。
test(between_enum, findall(X, between(1, 3, X), L), solutions([L = [1, 2, 3]])).
test(between_one, findall(X, between(1, 1, X), L), solutions([L = [1]])).
test(between_empty, between(1, 0, _), fail).
test(between_check_in, between(1, 5, 3), true).
test(between_check_last, findall(x, between(1, 5, 5), L), solutions([L = [x]])).
test(between_check_low, between(1, 3, 0), fail).
test(between_check_high, between(1, 3, 5), fail).
test(between_inf, between(1, inf, 5), true).
test(between_inf_enum, findall(X, (between(1, inf, X), (X >= 3, ! ; true)), L), solutions([L = [1, 2, 3]])).
test(between_inf_low, between(1, inf, 0), fail).
test(between_low_inst, between(_, 3, _), error(instantiation_error)).
test(between_high_inst, between(1, _, _), error(instantiation_error)).
test(between_type, between(a, 3, _), error(type_error(integer, a))).
test(between_high_type, between(1, a, _), error(type_error(integer, a))).
test(between_x_type, between(1, 3, a), error(type_error(integer, a))).
% succ/2。負の数は iso-notes の Section 5 succ/2 の決定 A。
test(succ_up, succ(1, N), solutions([N = 2])).
test(succ_down, succ(N, 2), solutions([N = 1])).
test(succ_check, findall(x, succ(1, 2), L), solutions([L = [x]])).
test(succ_wrong, succ(3, 2), fail).
test(succ_zero, succ(_, 0), fail).
test(succ_inst, succ(_, _), error(instantiation_error)).
test(succ_type, succ(a, _), error(type_error(integer, a))).
test(succ_type_2, succ(_, a), error(type_error(integer, a))).
test(succ_neg, succ(_, -1), error(domain_error(not_less_than_zero, -1))).
test(succ_neg_1, succ(-1, _), error(domain_error(not_less_than_zero, -1))).
test(succ_overflow, (current_prolog_flag(max_integer, M), succ(M, _)), error(evaluation_error(int_overflow))).
% plus/3。
test(plus_modes, plus(1, X, 3), solutions([X = 2])).
test(plus_sum, plus(1, 2, Z), solutions([Z = 3])).
test(plus_first, plus(X, 2, 3), solutions([X = 1])).
test(plus_check, findall(x, plus(1, 2, 3), L), solutions([L = [x]])).
test(plus_wrong, plus(1, 2, 4), fail).
test(plus_inst, plus(_, _, _), error(instantiation_error)).
test(plus_inst_1, plus(1, _, _), error(instantiation_error)).
test(plus_inst_2, plus(_, 1, _), error(instantiation_error)).
test(plus_inst_3, plus(_, _, 1), error(instantiation_error)).
test(plus_type, plus(a, 1, _), error(type_error(integer, a))).
test(plus_type_2, plus(1, a, _), error(type_error(integer, a))).
test(plus_type_3, plus(_, 1, a), error(type_error(integer, a))).
test(plus_overflow, (current_prolog_flag(max_integer, M), plus(M, 1, _)), error(evaluation_error(int_overflow))).
