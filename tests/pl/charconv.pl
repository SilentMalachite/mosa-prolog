test(conv_flag_off, current_prolog_flag(char_conversion, off), true).
test(conv_flag_bad, set_prolog_flag(char_conversion, true), error(domain_error(flag_value, char_conversion+true))).
test(conv_off, (set_prolog_flag(char_conversion, off), char_conversion(a, b), open_string('a.', S), read(S, T), T == a, char_conversion(a, a)), true).
test(conv_on, (set_prolog_flag(char_conversion, on), char_conversion(a, b), open_string('a.', S), read(S, T), T == b, char_conversion(a, a), set_prolog_flag(char_conversion, off)), true).
test(conv_quoted, (set_prolog_flag(char_conversion, on), char_conversion(a, b), open_string('\'a\'.', S), read(S, T), T == a, char_conversion(a, a), set_prolog_flag(char_conversion, off)), true).
test(conv_string, (set_prolog_flag(char_conversion, on), char_conversion(a, b), open_string([34, 97, 34, 46], S), read(S, T), T == [97], char_conversion(a, a), set_prolog_flag(char_conversion, off)), true).
test(conv_comment, (set_prolog_flag(char_conversion, on), char_conversion(a, b), open_string('/*a*/a.', S), read(S, T), T == b, char_conversion(a, a), set_prolog_flag(char_conversion, off)), true).
test(conv_code, (set_prolog_flag(char_conversion, on), char_conversion(a, b), open_string([48, 39, 97, 46], S), read(S, T), T == 98, char_conversion(a, a), set_prolog_flag(char_conversion, off)), true).
test(conv_pair, (set_prolog_flag(char_conversion, on), char_conversion(a, b), char_conversion(b, c), open_string('a.', S), read(S, A), open_string('b.', S2), read(S2, B), A == b, B == c, char_conversion(a, a), char_conversion(b, b), set_prolog_flag(char_conversion, off)), true).
test(conv_comma, (set_prolog_flag(char_conversion, on), char_code(Amp, 38), char_code(Comma, 44), char_conversion(Amp, Comma), open_string('x & y.', S), read(S, T), T == (x, y), char_conversion(Amp, Amp), set_prolog_flag(char_conversion, off)), true).
test(conv_back, (set_prolog_flag(char_conversion, on), char_conversion(a, b), char_conversion(a, a), open_string('a.', S), read(S, T), T == a, set_prolog_flag(char_conversion, off)), true).
test(conv_in_var, char_conversion(X, a), error(instantiation_error)).
test(conv_in_long, char_conversion(ab, a), error(type_error(character, ab))).
test(conv_in_before_out, char_conversion(ab, X), error(type_error(character, ab))).
test(conv_out_var, char_conversion(a, X), error(instantiation_error)).
test(conv_out_long, char_conversion(a, ab), error(type_error(character, ab))).
test(conv_in_int, char_conversion(1, a), error(type_error(character, 1))).
test(conv_in_empty, char_conversion('', a), error(type_error(character, ''))).
test(conv_in_fun, char_conversion(f(1), a), error(type_error(character, f(1)))).
test(conv_clear, (char_conversion(a, a), char_conversion(b, b), set_prolog_flag(char_conversion, off)), true).
