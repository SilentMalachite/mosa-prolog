% SPEC 第4節「Minimum set of Japanese tests」と、それを確かめるテスト（計画 Task 16）。
% - 日本語の原子と演算子の読み書きの往復: wq_tokyo、op_ha、op_ha_roundtrip、read_oya、write_oya
% - sub_atom/5 で 日本語処理系 から 語処: sub_atom_go_sho
% - atom_length('𠮷野家', 3): atom_length_yoshinoya
% - 日本語のファイル名の consult/1 と、日本語を含む syntax_error: consult_japanese_name、syntax_token_japanese、
%   syntax_col_japanese（表示の文面は tests/cpp/session_test.cpp の syntax_ja_shown と syntax_en_shown）
% - 全角記号の旗の有無で読みの結果が同じ: fw_same_term
:- use_module(library(japanese)).
:- use_module(library(format)).
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
test(op_ha_roundtrip, (op(700, xfx, は), with_output_to(atom(A), writeq(は(太郎, 学生))), A == '太郎 は 学生', atom_concat(A, '.', Text), open_string(Text, S), read(S, T), T == は(太郎, 学生), op(0, xfx, は)), true).
test(wq_tokyo, (with_output_to(atom(A), writeq(東京)), A == '東京'), true).
test(wq_tokyo_space, (with_output_to(atom(A), writeq('東 京')), A == '''東 京'''), true).
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
% 全角記号モード（SPEC 第4節、iso-notes 第4節）。旗は読む間だけ真にし、例外のときも偽に戻す。
全角で読む(Text, T) :-
    set_prolog_flag(japanese_punctuation, true),
    catch((open_string(Text, S), read(S, T0)), E, true),
    set_prolog_flag(japanese_punctuation, false),
    ( nonvar(E) -> throw(E) ; T = T0 ).
test(fw_same_term, (全角で読む('親（太郎，一郎）。', T1), open_string('親(太郎,一郎).', S), read(S, T2), T1 == T2), true).
test(fw_list, (全角で読む('X = ［1，2］。', T), T = (_ = L), L == [1, 2]), true).
test(fw_touten, (全角で読む('f（a、b）。', T), T == f(a, b)), true).
test(fw_colon, (全角で読む('a：b。', T), T == (a:b)), true).
test(fw_kagi, (全角で読む('X = 「東 京」。', T), T = (_ = A), A == '東 京', with_output_to(atom(W), writeq(A)), W == '''東 京'''), true).
test(fw_kagi_escape, (全角で読む('X = 「a\\nb」。', T), T = (_ = A), atom_codes(A, [0'a, 10, 0'b])), true).
test(fw_kagi_doubled, (全角で読む('X = 「a」」b」。', T), T = (_ = A), A == 'a」b'), true).
test(fw_kagi_apostrophe, (全角で読む('X = 「it''s」。', T), T = (_ = A), A == 'it''s'), true).
test(fw_kagi_newline, 全角で読む([0'X, 0' , 0'=, 0' , 0'「, 0'a, 10, 0'b, 0'」, 0'。], _), error(syntax_error(_))).
test(fw_kuten_not_decimal, (全角で読む('X = 1。', T), T = (_ = N), N == 1), true).
test(fw_kuten_no_float, 全角で読む('X = 1。5。', _), error(syntax_error(_))).
test(fw_off_symbol, (open_string('X = 、.', S), read(S, T), T = (_ = A), A == '、', open_string('X = 。.', S2), read(S2, T2), T2 = (_ = B), B == '。'), true).
test(fw_quoted_kept, (全角で読む('X = f（''（）''，"，"）。', T), T = (_ = f(A, C)), A == '（）', atom_codes('，', C)), true).
test(fw_char_code_kept, (全角で読む('X = 0''（。', T), T = (_ = N), char_code('（', N)), true).
test(fw_comment_kept, (全角で読む('X = a /* （ */。', T), T = (_ = A), A == a), true).
:- consult('modules/全角.pl').
test(fw_directive_next_clause, 全(1), true).
test(fw_directive_per_file, current_prolog_flag(japanese_punctuation, false), true).
% SPEC 第4節。構文エラーの列は文字で数える。全角の字も 1 列。
test(syntax_token_japanese, (open_string('子 親.', S), catch(read(S, _), error(syntax_error(_), context(_, syntax(_, Token))), true), Token == '親'), true).
test(syntax_col_japanese, (open_string('あいう ).', S1), catch(read(S1, _), error(_, context(_, syntax(stream(_, _, P1, _), _))), true), open_string('abc ).', S2), catch(read(S2, _), error(_, context(_, syntax(stream(_, _, P2, _), _))), true), P1 == P2), true).
% ~t~| の桁は東アジア幅で数える（計画 Task 13）。日本 は幅 4、日a は幅 3。
test(fmt_wide_pad, with_output_to(atom(A), format("~w~t~10|~w", [日本, x])), solutions([A = '日本      x'])).
test(fmt_ascii_pad, with_output_to(atom(A), format("~w~t~10|~w", [ab, x])), solutions([A = 'ab        x'])).
test(fmt_mixed, with_output_to(atom(A), format("~w~t~10|~w", [日a, x])), solutions([A = '日a       x'])).
% 埋め草の字も幅で数える。幅 2 の字で奇数の桁が余れば空白で埋め、桁位置をそろえる（iso-notes の決定 A）。
test(fmt_wide_fill_even, with_output_to(atom(A), format("~`漢t~4|x", [])), solutions([A = '漢漢x'])).
test(fmt_wide_fill_odd, with_output_to(atom(A), format("~`漢t~5|x", [])), solutions([A = '漢漢 x'])).
test(fmt_wide_fill_after_text, with_output_to(atom(A), format("ab~`漢t~6|x", [])), solutions([A = 'ab漢漢x'])).
test(fmt_wide_fill_two, with_output_to(atom(A), format("~`漢t~w~`漢t~7|", [ab])), solutions([A = '漢ab漢 '])).
% SPEC 第4節。日本語のファイル名を書いて consult できる（計画 Task 15）。パスはリポジトリの根から。
test(consult_japanese_name, (open('tests/tmp/日本語の読込.pl', write, S), write(S, '読込済(はい).'), nl(S), close(S), consult('tests/tmp/日本語の読込.pl'), 読込済(はい)), true).
