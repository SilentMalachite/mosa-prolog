:- set_prolog_flag(char_conversion, on).
:- char_conversion(q, z).
:- set_prolog_flag(char_conversion, off).

test(conv_from_directive, (set_prolog_flag(char_conversion, on), open_string('q.', S), read(S, T), T == z, char_conversion(q, q), set_prolog_flag(char_conversion, off)), true).
