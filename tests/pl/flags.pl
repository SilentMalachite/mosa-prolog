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
test(flag_ja_punct_set, (set_prolog_flag(japanese_punctuation, true), current_prolog_flag(japanese_punctuation, V), set_prolog_flag(japanese_punctuation, false)), solutions([V = true])).

% iso-notes「Section 3 Flags dialect and version_data」の決定 B。どちらも変えられない処理系定義の旗。
test(flag_dialect, current_prolog_flag(dialect, mosa), true).
test(flag_version_data, current_prolog_flag(version_data, mosa(0, 0, 1, [])), true).
test(flag_dialect_listed, (current_prolog_flag(F, mosa), F == dialect), true).
test(flag_dialect_modify, set_prolog_flag(dialect, swi), error(permission_error(modify, flag, dialect))).
test(flag_dialect_value, set_prolog_flag(dialect, 1), error(domain_error(flag_value, dialect+1))).
test(flag_version_data_modify, set_prolog_flag(version_data, mosa(9, 9, 9, [])), error(permission_error(modify, flag, version_data))).
test(flag_version_data_value, set_prolog_flag(version_data, x), error(domain_error(flag_value, version_data+x))).

% SPEC 第3節 7 の旗 version。値は Major*10000 + Minor*100 + Patch の整数で、変えられない（持ち主の決定 A、2026-10-28）。
test(flag_version, current_prolog_flag(version, 1), true).
test(flag_version_listed, (current_prolog_flag(F, 1), F == version), true).
test(flag_version_modify, set_prolog_flag(version, 2), error(permission_error(modify, flag, version))).
test(flag_version_value, set_prolog_flag(version, x), error(domain_error(flag_value, version+x))).
% 旗 message_language（ADR 0007）。既定は LANG で決まるので、値は ja か en のどちらか。
test(ml_current, (current_prolog_flag(message_language, V), (V == ja ; V == en)), true).
test(ml_set, (current_prolog_flag(message_language, Old), set_prolog_flag(message_language, en), current_prolog_flag(message_language, V), set_prolog_flag(message_language, Old)), solutions([V = en])).
test(ml_values, set_prolog_flag(message_language, fr), error(domain_error(flag_value, message_language+fr))).
