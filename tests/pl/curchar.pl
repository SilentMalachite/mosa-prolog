test(ccc_none, (findall(In-Out, current_char_conversion(In, Out), L), L == [], \+ current_char_conversion(a, a), \+ current_char_conversion(a, X)), true).
test(ccc_ground, (char_conversion(a, b), current_char_conversion(a, b), char_conversion(a, a)), true).
test(ccc_in, (char_conversion(a, b), current_char_conversion(a, X), X == b, char_conversion(a, a)), true).
test(ccc_out, (char_conversion(a, b), current_char_conversion(X, b), X == a, char_conversion(a, a)), true).
test(ccc_miss, (char_conversion(a, b), \+ current_char_conversion(a, c), \+ current_char_conversion(c, b), \+ current_char_conversion(a, a), char_conversion(a, a)), true).
test(ccc_same, (char_conversion(a, b), \+ current_char_conversion(X, X), char_conversion(a, a)), true).
test(ccc_flag_off, (set_prolog_flag(char_conversion, off), char_conversion(a, b), current_char_conversion(a, b), open_string('a.', S), read(S, T), T == a, char_conversion(a, a)), true).
test(ccc_order, (char_conversion(a, b), char_conversion(c, d), findall(In-Out, current_char_conversion(In, Out), L), L == [a-b, c-d], char_conversion(a, a), char_conversion(c, c)), true).
test(ccc_replace, (char_conversion(a, b), char_conversion(c, d), char_conversion(a, e), findall(In-Out, current_char_conversion(In, Out), L), L == [c-d, a-e], char_conversion(a, a), char_conversion(c, c)), true).
test(ccc_many, (char_conversion(a, c), char_conversion(b, c), findall(In, current_char_conversion(In, c), L), L == [a, b], findall(Out, current_char_conversion(a, Out), One), One == [c], char_conversion(a, a), char_conversion(b, b)), true).
test(ccc_compose, (char_conversion(a, b), char_conversion(b, c), current_char_conversion(a, b), current_char_conversion(b, c), \+ current_char_conversion(a, c), char_conversion(a, a), char_conversion(b, b)), true).
test(ccc_clear, (char_conversion(a, b), char_conversion(a, a), findall(In-Out, current_char_conversion(In, Out), L), L == []), true).
test(ccc_snap, (char_conversion(a, x), char_conversion(b, y), findall(In, (current_char_conversion(In, _), char_conversion(b, b)), L), L == [a, b], char_conversion(a, a), char_conversion(b, b)), true).
test(ccc_comma, (char_code(Amp, 38), char_code(Comma, 44), char_conversion(Amp, Comma), current_char_conversion(Amp, Out), Out == Comma, current_char_conversion(In, Comma), In == Amp, char_conversion(Amp, Amp)), true).
test(ccc_in_long, current_char_conversion(ab, a), error(type_error(character, ab))).
test(ccc_in_before_out, current_char_conversion(ab, X), error(type_error(character, ab))).
test(ccc_out_long, current_char_conversion(a, ab), error(type_error(character, ab))).
test(ccc_in_int, current_char_conversion(1, a), error(type_error(character, 1))).
test(ccc_out_int, current_char_conversion(a, 1), error(type_error(character, 1))).
test(ccc_in_before_bad_out, current_char_conversion(1, ab), error(type_error(character, 1))).
test(ccc_var_out, current_char_conversion(X, 1), error(type_error(character, 1))).
test(ccc_in_empty, current_char_conversion('', a), error(type_error(character, ''))).
test(ccc_out_empty, current_char_conversion(a, ''), error(type_error(character, ''))).
test(ccc_in_fun, current_char_conversion(f(1), a), error(type_error(character, f(1)))).
test(ccc_out_fun, current_char_conversion(a, f(1)), error(type_error(character, f(1)))).
test(ccc_in_list, current_char_conversion([a], a), error(type_error(character, [a]))).
test(ccc_in_float, current_char_conversion(1.0, a), error(type_error(character, 1.0))).
test(ccc_keeps, (char_conversion(a, b), catch((current_char_conversion(ab, X), fail), error(type_error(character, ab), _), true), current_char_conversion(a, b), char_conversion(a, a)), true).
test(ccc_done, (char_conversion(a, a), char_conversion(b, b), char_conversion(c, c), char_conversion(d, d), char_conversion(e, e), char_conversion(x, x), char_conversion(y, y), char_code(Amp, 38), char_conversion(Amp, Amp), set_prolog_flag(char_conversion, off)), true).
