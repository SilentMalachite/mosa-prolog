:- use_module(library(japanese)).
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

test(ensure_ha, ensure_loaded(含む), error(existence_error(procedure, ensure_loaded/1))).
% library(japanese)。どちらの述語も、第1引数が決まっていれば前向き、第2引数だけ決まっていれば逆向きに写す。
% 入力は原子か数、結果は変数か原子（library(strings) と同じ）。
test(hk_basic, ひらがな_カタカナ(さくら, X), solutions([X = サクラ])).
test(hk_reverse, ひらがな_カタカナ(X, サクラ), solutions([X = さくら])).
test(hk_mixed, ひらがな_カタカナ('さくらA', X), solutions([X = 'サクラA'])).
test(hk_edges, ひらがな_カタカナ('ぁゖ', X), solutions([X = 'ァヶ'])).
test(hk_outside, ひらがな_カタカナ('ゝー漢', X), solutions([X = 'ゝー漢'])).
test(hk_katakana_kept, ひらがな_カタカナ('サくら', X), solutions([X = サクラ])).
test(hk_check, ひらがな_カタカナ(さくら, サクラ), true).
test(hk_check_fail, ひらがな_カタカナ(さくら, さくら), fail).
test(hk_empty, ひらがな_カタカナ('', X), solutions([X = ''])).
test(hk_det, findall(X, ひらがな_カタカナ(あ, X), L), solutions([L = [ア]])).
test(hk_inst, ひらがな_カタカナ(_, _), error(instantiation_error)).
test(hk_type, ひらがな_カタカナ(f(x), _), error(type_error(atomic, f(x)))).
test(hk_type_out, ひらがな_カタカナ(あ, f(x)), error(type_error(atom, f(x)))).
test(hk_type_rev, ひらがな_カタカナ(_, f(x)), error(type_error(atomic, f(x)))).
test(zh_basic, 全角_半角('ＡＢ１', X), solutions([X = 'AB1'])).
test(zh_space, 全角_半角('Ａ　Ｂ', X), solutions([X = 'A B'])).
test(zh_edges, 全角_半角('！～', X), solutions([X = '!~'])).
test(zh_kept, 全角_半角('ｱ漢a', X), solutions([X = 'ｱ漢a'])).
test(zh_reverse, 全角_半角(X, 'A b!'), solutions([X = 'Ａ　ｂ！'])).
test(zh_check, 全角_半角('ＡＢ', 'AB'), true).
test(zh_number, 全角_半角(12, X), solutions([X = '12'])).
test(zh_inst, 全角_半角(_, _), error(instantiation_error)).
test(zh_type, 全角_半角(f(x), _), error(type_error(atomic, f(x)))).
test(width_mixed, 読み幅('日本a', 5), true).
test(width_scalar, 読み幅('𠮷', 2), true).
test(width_ascii, 読み幅(abc, W), solutions([W = 3])).
test(width_fullwidth, 読み幅('ＡＢ　', W), solutions([W = 6])).
test(width_halfwidth_kana, 読み幅('ｱｲ', W), solutions([W = 2])).
test(width_empty, 読み幅('', W), solutions([W = 0])).
test(width_number, 読み幅(123, W), solutions([W = 3])).
test(width_check_fail, 読み幅('日本', 3), fail).
test(width_inst, 読み幅(_, _), error(instantiation_error)).
test(width_type, 読み幅(f(x), _), error(type_error(atomic, f(x)))).
test(width_type_out, 読み幅(a, x), error(type_error(integer, x))).
% '$char_width'/2 は system にある。
test(char_width_wide, '$char_width'(0'漢, W), solutions([W = 2])).
test(char_width_narrow, '$char_width'(0'a, W), solutions([W = 1])).
test(char_width_inst, '$char_width'(_, _), error(instantiation_error)).
test(char_width_type, '$char_width'(a, _), error(type_error(integer, a))).
test(char_width_repr, '$char_width'(-1, _), error(representation_error(character_code))).
