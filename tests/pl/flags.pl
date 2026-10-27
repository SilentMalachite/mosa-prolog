test(flag_bounded, current_prolog_flag(bounded, true), true).
test(flag_rounding, current_prolog_flag(integer_rounding_function, toward_zero), true).
test(flag_max_integer, current_prolog_flag(max_integer, 9223372036854775807), true).
test(flag_min_integer, (current_prolog_flag(min_integer, X), Y is -9223372036854775807 - 1, X =:= Y), true).
test(flag_max_arity, current_prolog_flag(max_arity, 1024), true).
test(flag_double_quotes, current_prolog_flag(double_quotes, codes), true).
test(flag_unknown, current_prolog_flag(unknown, error), true).
test(flag_encoding, current_prolog_flag(encoding, utf8), true).
test(flag_ja_punct, current_prolog_flag(japanese_punctuation, false), true).
test(flag_float_overflow, current_prolog_flag(float_overflow, error), true).
test(flag_float_zero_div, current_prolog_flag(float_zero_div, error), true).
test(flag_float_undefined, current_prolog_flag(float_undefined, error), true).
test(flag_debug, current_prolog_flag(debug, off), true).
test(flag_debug_on, (set_prolog_flag(debug, on), current_prolog_flag(debug, on), set_prolog_flag(debug, off), current_prolog_flag(debug, off)), true).
test(flag_debug_bad, set_prolog_flag(debug, drop), error(domain_error(flag_value, debug+drop))).
test(unknown_fail, (set_prolog_flag(unknown, fail), no_such_pred_phase3), fail).
test(unknown_restored, (set_prolog_flag(unknown, error), no_such_pred_phase3), error(existence_error(procedure, no_such_pred_phase3/0))).
test(flag_bounded_value, set_prolog_flag(bounded, foo), error(domain_error(flag_value, bounded+foo))).
test(flag_bounded_same, set_prolog_flag(bounded, true), error(permission_error(modify, flag, bounded))).
test(flag_max_arity_modify, set_prolog_flag(max_arity, 40), error(permission_error(modify, flag, max_arity))).
test(flag_max_arity_atom, set_prolog_flag(max_arity, foo), error(domain_error(flag_value, max_arity+foo))).
test(flag_quotes_bad, set_prolog_flag(double_quotes, foo), error(domain_error(flag_value, double_quotes+foo))).
