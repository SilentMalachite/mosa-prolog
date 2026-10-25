test(atom_length_hiragana, atom_length('あ', 1), true).
test(atom_length_scalar, atom_length('𠮷', 1), true).
test(atom_length_yoshinoya, atom_length('𠮷野家', 3), true).
test(char_code_scalar, char_code('𠮷', 134071), true).
test(sub_atom_go_sho, sub_atom('日本語処理系', 2, 2, 2, '語処'), true).
test(atom_length_arg_order, atom_length(X, foo), error(instantiation_error)).
test(open_string_oya, (open_string('親', S), functor(S, '$stream', 1)), true).
test(open_string_scalar, (open_string('𠮷', S), functor(S, '$stream', 1)), true).
test(open_string_scalar_code, (open_string([134071], S), functor(S, '$stream', 1)), true).
test(get_oya, (open_string('親', S), get_char(S, '親')), true).
test(peek_scalar, (open_string('𠮷', S), peek_char(S, C), get_char(S, D), C == '𠮷', D == '𠮷'), true).
test(seek_oya, (open_string('親あ', S), get_char(S, '親'), set_stream_position(S, stream_position(0, 1, 1)), get_char(S, '親')), true).
test(get_code_oya, (open_string('親', S), get_code(S, C), char_code('親', C)), true).
test(peek_code_scalar, (open_string('𠮷', S), peek_code(S, C), get_code(S, D), C == D, char_code('𠮷', C)), true).
test(put_code_oya_type, put_code('親'), error(type_error(integer, '親'))).
test(read_oya, (open_string('親.', S), read(S, '親'), stream_property(S, position(stream_position(2, 1, 3)))), true).
test(write_oya, (with_output_to(atom(A), write(親)), A == 親), true).
test(op_ha, (op(700, xfx, は), open_string('太郎 は 学生.', S), read(S, T), T == は(太郎, 学生), op(0, xfx, は)), true).
test(cop_ha, (op(700, xfx, は), current_op(700, xfx, は), op(0, xfx, は)), true).
test(conv_ha, (set_prolog_flag(char_conversion, on), char_conversion(あ, い), open_string('あ.', S), read(S, T), T == い, char_conversion(あ, あ), set_prolog_flag(char_conversion, off)), true).
test(ccc_ha, (\+ current_char_conversion(あ, あ), char_conversion(あ, い), current_char_conversion(あ, い), current_char_conversion(X, い), X == あ, findall(P-Q, current_char_conversion(P, Q), L), L == [あ-い], char_conversion(あ, あ)), true).

:- discontiguous(離/1).

離(1).
続(1).
離(2).

test(dc_ha, (findall(X, 離(X), L), L == [1, 2]), true).

:- multifile(複/1).

複(1).
複(2).

test(mf_ha, (findall(X, 複(X), L), L == [1, 2]), true).

:- dynamic(初/0).
:- initialization((retractall(初), assertz(初))).

test(init_ha, 初, true).

test(include_ha, include(含む), error(existence_error(procedure, include/1))).
