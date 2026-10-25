test(atom_length_evening, atom_length('enchanted evening', N), solutions([N = 17])).
test(atom_length_evening_n, atom_length('enchanted evening', 17), true).
test(atom_length_empty, atom_length('', N), solutions([N = 0])).
test(atom_concat_hello, atom_concat('hello', ' world', A), solutions([A = 'hello world'])).
test(atom_concat_small, atom_concat(A, ' world', 'small world'), solutions([A = small])).
test(atom_concat_world, atom_concat('small', A, 'small world'), solutions([A = ' world'])).
test(atom_concat_split, atom_concat(A1, A2, hello), solutions([
    (A1 = '', A2 = hello),
    (A1 = h, A2 = ello),
    (A1 = he, A2 = llo),
    (A1 = hel, A2 = lo),
    (A1 = hell, A2 = o),
    (A1 = hello, A2 = '')
])).
test(atom_concat_open, atom_concat(A, B, C), error(instantiation_error)).
test(sub_atom_left, sub_atom(abracadabra, 0, 5, A, S), solutions([(A = 6, S = abrac)])).
test(sub_atom_right, sub_atom(abracadabra, B, 5, 0, S), solutions([(B = 6, S = dabra)])).
test(sub_atom_abra, sub_atom(abracadabra, B, 4, A, abra), solutions([
    (B = 0, A = 7),
    (B = 7, A = 0)
])).
test(sub_atom_ab, sub_atom(ab, B, L, A, S), solutions([
    (B = 0, L = 0, A = 2, S = ''),
    (B = 0, L = 1, A = 1, S = a),
    (B = 0, L = 2, A = 0, S = ab),
    (B = 1, L = 0, A = 1, S = ''),
    (B = 1, L = 1, A = 0, S = b),
    (B = 2, L = 0, A = 0, S = '')
])).
test(sub_atom_open, sub_atom(Atom, 0, 1, 0, S), error(instantiation_error)).
test(atom_chars_empty, atom_chars('', L), solutions([L = []])).
test(atom_chars_out, atom_chars(abc, L), solutions([L = [a, b, c]])).
test(atom_chars_in, atom_chars(X, [a, b, c]), solutions([X = abc])).
test(atom_chars_open, atom_chars(X, Y), error(instantiation_error)).
test(atom_codes_empty, atom_codes('', L), solutions([L = []])).
test(atom_codes_out, atom_codes(abc, L), solutions([L = [97, 98, 99]])).
test(atom_codes_in, atom_codes(X, [97, 98, 99]), solutions([X = abc])).
test(char_code_out, char_code(a, C), solutions([C = 97])).
test(char_code_in, char_code(C, 99), solutions([C = c])).
test(char_code_open, char_code(C, Code), error(instantiation_error)).
test(number_chars_int, number_chars(33, L), solutions([L = ['3', '3']])).
test(number_chars_in, number_chars(X, ['3', '3']), solutions([X = 33])).
test(number_chars_neg, number_chars(-25, L), solutions([L = ['-', '2', '5']])).
test(number_chars_neg_in, number_chars(X, ['-', '2', '5']), solutions([X = -25])).
test(number_chars_float, number_chars(33.0, L), solutions([L = ['3', '3', '.', '0']])).
test(number_chars_float_in, number_chars(X, ['3', '.', '3']), solutions([X = 3.3])).
test(number_codes_int, number_codes(33, L), solutions([L = [51, 51]])).
test(number_codes_in, number_codes(X, [51, 51]), solutions([X = 33])).
test(number_codes_neg, number_codes(X, [45, 50, 53]), solutions([X = -25])).
