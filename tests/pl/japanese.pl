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

use_japanese :- use_module(library(japanese)).

% 取り込むまで user からは見えない。
test(japanese_kana_hidden, ひらがな_カタカナ(あ, _), error(existence_error(procedure, ひらがな_カタカナ/2))).
test(japanese_width_hidden, 読み幅(あ, _), error(existence_error(procedure, 読み幅/2))).
test(japanese_width_pair_hidden, 全角_半角('Ａ', _), error(existence_error(procedure, 全角_半角/2))).

% ひらがなの符号位置は 0x60 足してカタカナになる。
test(japanese_kana, (use_japanese, ひらがな_カタカナ(あ, K), K == ア), true).

% カタカナ側だけが具体値なら、0x60 引いてひらがなに戻す。
test(japanese_kana_back, (use_japanese, ひらがな_カタカナ(H, ア), H == あ), true).

% 範囲の端も写す。長音と ASCII はそのまま。
test(japanese_kana_edge, (use_japanese, ひらがな_カタカナ('ぁゖーa', K), K == 'ァヶーa'), true).
test(japanese_kana_edge_back, (use_japanese, ひらがな_カタカナ(H, 'ァヶーa'), H == 'ぁゖーa'), true).

% 逆向きの範囲には入らない符号位置は、その向きでは変えない。
test(japanese_kana_keep_kata, (use_japanese, ひらがな_カタカナ(ア, K), K == ア), true).
test(japanese_kana_keep_hira, (use_japanese, ひらがな_カタカナ(H, あ), H == あ), true).

% 両方具体値で、写した値と違えば失敗する。
test(japanese_kana_mismatch, (use_japanese, ひらがな_カタカナ(あ, あ)), fail).

test(japanese_kana_var, (use_japanese, ひらがな_カタカナ(_, _)), error(instantiation_error)).
test(japanese_kana_type, (use_japanese, ひらがな_カタカナ(1, _)), error(type_error(atom, 1))).
test(japanese_kana_type_back, (use_japanese, ひらがな_カタカナ(_, 1)), error(type_error(atom, 1))).

% 東アジア幅の W または F は 2。ひらがなは W。
test(japanese_width, (use_japanese, 読み幅(あ, W), W == 2), true).

% ASCII は 1。全角の ASCII は F で 2。半角カタカナと曖昧幅は 1。
test(japanese_width_ascii, (use_japanese, 読み幅(a, W), W == 1), true).
test(japanese_width_sum, (use_japanese, 読み幅('あa', W), W == 3), true).
test(japanese_width_empty, (use_japanese, 読み幅('', W), W == 0), true).
test(japanese_width_full, (use_japanese, 読み幅('Ａ', W), W == 2), true).
test(japanese_width_half, (use_japanese, 読み幅('ｱ', W), W == 1), true).
test(japanese_width_ambiguous, (use_japanese, 読み幅('α', W), W == 1), true).

% 両方具体値で幅が違えば失敗する。
test(japanese_width_mismatch, (use_japanese, 読み幅(あ, 1)), fail).

test(japanese_width_var, (use_japanese, 読み幅(_, _)), error(instantiation_error)).
test(japanese_width_type, (use_japanese, 読み幅(1, _)), error(type_error(atom, 1))).
test(japanese_width_width_type, (use_japanese, 読み幅(あ, a)), error(type_error(integer, a))).

% 全角の ASCII は 0xFEE0 引いて半角になる。
test(japanese_full, (use_japanese, 全角_半角('Ａ', H), H == 'A'), true).

% 半角側だけが具体値なら、0xFEE0 足して全角にする。
test(japanese_full_back, (use_japanese, 全角_半角(F, 'A'), F == 'Ａ'), true).

% 半角カタカナは、その互換分解の全角へ写す。逆は半角へ戻す。
test(japanese_half_kata, (use_japanese, 全角_半角(F, 'ｶ'), F == 'カ'), true).
test(japanese_half_kata_back, (use_japanese, 全角_半角('カ', H), H == 'ｶ'), true).
test(japanese_half_stop, (use_japanese, 全角_半角(F, '｡'), F == '。'), true).
test(japanese_half_mark, (use_japanese, 全角_半角(F, 'ﾞ'), atom_codes(F, [0x3099])), true).
test(japanese_half_mark_back, (use_japanese, atom_codes(F, [0x3099]), 全角_半角(F, H), H == 'ﾞ'), true).
test(japanese_half_long, (use_japanese, 全角_半角('ー', H), H == 'ｰ'), true).
test(japanese_half_mix, (use_japanese, 全角_半角(F, 'Aｶ'), F == 'Ａカ'), true).

% 全角空白と ASCII の端。対の無い符号位置はそのまま。
test(japanese_space, (use_japanese, 全角_半角('　', H), H == ' '), true).
test(japanese_space_back, (use_japanese, 全角_半角(F, ' '), F == '　'), true).
test(japanese_full_edge, (use_japanese, 全角_半角('！～', H), H == '!~'), true).
test(japanese_full_keep, (use_japanese, 全角_半角(あ, H), H == あ), true).
test(japanese_full_mismatch, (use_japanese, 全角_半角('カ', 'カ')), fail).

test(japanese_full_var, (use_japanese, 全角_半角(_, _)), error(instantiation_error)).
test(japanese_full_type, (use_japanese, 全角_半角(1, _)), error(type_error(atom, 1))).
test(japanese_full_type_back, (use_japanese, 全角_半角(_, 1)), error(type_error(atom, 1))).
