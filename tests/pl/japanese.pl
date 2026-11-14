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
test(fw_end_percent, (全角で読む('w（x）。% c', T), T == w(x)), true).
test(fw_end_percent_next, (set_prolog_flag(japanese_punctuation, true), catch((open_string('w（x）。%c\nv（y）。', S), read(S, A), read(S, B)), E, true), set_prolog_flag(japanese_punctuation, false), var(E), A == w(x), B == v(y)), true).
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
% 旗が偽なら 、 も 。 も図形の字で、続けた . は同じトークンに入る（6.4.2、レビュー L17）。空白で終止符を分ける。
test(fw_off_symbol, (open_string('X = 、 .', S), read(S, T), T = (_ = A), A == '、', open_string('X = 。 .', S2), read(S2, T2), T2 = (_ = B), B == '。'), true).
test(fw_quoted_kept, (全角で読む('X = f（''（）''，"，"）。', T), T = (_ = f(A, C)), A == '（）', atom_codes('，', C)), true).
test(fw_char_code_kept, (全角で読む('X = 0''（。', T), T = (_ = N), char_code('（', N)), true).
test(fw_comment_kept, (全角で読む('X = a /* （ */。', T), T = (_ = A), A == a), true).
% レビュー L17。全角記号モードの 。 も . と同じく、トークンの先頭のときだけ終止符で、図形の字に続けば同じトークンに入る。
test(l17_fw_plus_kuten, (全角で読む('X = +。 。', T), T = (_ = A)), solutions([A = '+。'])).
test(l17_fw_space_kuten, (全角で読む('X = + 。', T), T = (_ = A)), solutions([A = (+)])).
test(l17_fw_plus_kuten_eof, 全角で読む('X = +。', _), error(syntax_error(incomplete_term))).
test(l17_fw_off_glued, (open_string('X = 、.', S), read(S, _)), error(syntax_error(incomplete_term))).
% 全角記号モードの writeq（レビュー M8、7.10.5 h）。引用しないと半角に読まれる （），、： を含む図形の原子と、
% 後ろに空白が来ると終止符になる 。 で終わる図形の原子を引用する。
% 旗を真にして G を 1 度だけ走らせ、成功、失敗、例外のどれでも偽に戻す。真のまま残すと、実行器が後のテストの字面を全角で読む。
全角で(G) :-
    set_prolog_flag(japanese_punctuation, true),
    catch((G -> R = true ; R = false), E, R = error(E)),
    set_prolog_flag(japanese_punctuation, false),
    ( R = error(E2) -> throw(E2) ; R == true ).
全角で書く(T, W) :- 全角で(with_output_to(atom(W), writeq(T))).
% 書いた字面に ' .' を足して全角記号モードで読み、元の項と同じか見る。
全角で往復(T) :-
    全角で書く(T, W),
    atom_concat(W, ' .', Text),
    全角で読む(Text, T2),
    T2 == T.
test(m8_touten_arg, (全角で書く(f('、'), W), W == 'f(''、'')'), true).
test(m8_touten_arg_back, 全角で往復(f('、')), true).
test(m8_colon_functor, (全角で書く('：'(a), W), W == '''：''(a)'), true).
test(m8_colon_functor_back, 全角で往復('：'(a)), true).
test(m8_touten_functor_back, 全角で往復('、'(a, b)), true).
test(m8_paren_open, (全角で書く('（', W), W == '''（'''), true).
test(m8_paren_back, 全角で往復(f('（', '）')), true).
test(m8_comma_back, 全角で往復(['，', '、']), true).
test(m8_graphic_mixed, (全角で書く(['+、', '：-', '-（'], W), W == '[''+、'',''：-'',''-（'']'), true).
test(m8_graphic_mixed_back, 全角で往復(f('+、', '：-', '-（')), true).
test(m8_kuten, (全角で書く(['。', '+。'], W), W == '[''。'',''+。'']'), true).
test(m8_kuten_back, 全角で往復(f('。') - '+。'), true).
% 原子の途中の 。 の後ろは図形の字で、終止符にならない。読めるので引用しない。
test(m8_kuten_inner_bare, (全角で書く('。+', W), W == '。+'), true).
test(m8_kuten_inner_back, 全角で往復('。+' - a), true).
test(m8_brackets_kagi, (全角で書く(f('［', '］', '「', '」'), W), W == 'f(''［'',''］'',''「'',''」'')'), true).
test(m8_operator, (op(200, xfx, '：'), catch((全角で書く('：'(a, b), W) -> true ; W = none), E, W = error(E)), op(0, xfx, '：'), W == 'a''：''b'), true).
test(m8_operator_back, (op(200, xfx, '：'), catch((全角で往復('：'(a, b)) -> R = true ; R = false), E, R = error(E)), op(0, xfx, '：'), R == true), true).
test(m8_prefix_operand_back, 全角で往復(- '、'), true).
% 新しく引用する原子が引用した演算子と隣り合う（M8 の Codex レビュー）。'。''x y' は 1 つの原子に読まれる。
test(m8_quoted_operator_back, (op(200, xf, 'x y'), catch((全角で往復('x y'('。')) -> R = true ; R = false), E, R = error(E)), op(0, xf, 'x y'), R == true), true).
test(m8_zero_operator_back, (op(200, xfx, '。'), catch((全角で往復('。'(0, a)) -> R = true ; R = false), E, R = error(E)), op(0, xfx, '。'), R == true), true).
% 演算子の末尾の . の後の 0 も整数の字句（Codex レビュー 3 回目）。a..0'。'b の 0'。 は文字コードになる。
test(m8_dots_zero_back, (op(100, xfx, '..'), op(200, xfx, '。'), catch((全角で往復('。'('..'(a, 0), b)) -> R = true ; R = false), E, R = error(E)), op(0, xfx, '。'), op(0, xfx, '..'), R == true), true).
test(m8_canonical, (全角で(with_output_to(atom(W), write_canonical('、'(a)))), W == '''、''(a)'), true).
test(m8_write_term_format, (全角で((with_output_to(atom(P), write_term('：', [quoted(true)])), with_output_to(atom(Q), format("~q", ['）'])))), P == '''：''', Q == '''）'''), true).
% write/1 は引用しない。旗が偽の writeq も、そのまま読める字は引用しない（7.10.5 h）。
test(m8_write_unquoted, (全角で(with_output_to(atom(W), write(f('、')))), W == 'f(、)'), true).
test(m8_flag_off_bare, (with_output_to(atom(W), writeq(f('、', '（', '：', '。'))), W == 'f(、,（,：,。)'), true).
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
% ~+ の桁も東アジア幅で数える（フェーズ11のタスク9）。日本 は幅 4 なので 8 桁まで 4 桁、日a は幅 3 なので 6 桁まで 3 桁埋める。
test(fmt_plus_wide_1, with_output_to(atom(A), format("~a~t~8+|", ['日本'])), solutions([A = '日本    |'])).
test(fmt_plus_wide_2, with_output_to(atom(A), format("~t~a~6+|", ['日a'])), solutions([A = '   日a|'])).
% SPEC 第4節。日本語のファイル名を書いて consult できる（計画 Task 15）。パスはリポジトリの根から。
test(consult_japanese_name, (open('tests/tmp/日本語の読込.pl', write, S), write(S, '読込済(はい).'), nl(S), close(S), consult('tests/tmp/日本語の読込.pl'), 読込済(はい)), true).
% term_to_atom/2 は日本語の原子と変数名を読み書きする（レビュー M11）。
test(m11_term_to_atom_japanese, (term_to_atom(T, '親(太郎, _名前, _名前)'), T = 親(A, B, C), A == 太郎, var(B), B == C, term_to_atom(親(太郎, '次 郎'), S), S == '親(太郎,\'次 郎\')'), true).
% L17。旗が真の writeq が 。 で終わる原子を引用するのは今も正しい。+。 と +. を並べても読み戻せる。
test(l17_fw_back, 全角で往復(f('+。', '+.', - '#')), true).
% レビュー M32。日本語と 4 バイトの字でも、Sub の字面を探す位置と決まった引数の解は字の数で数える。
test(m32_ja_sub, sub_atom('日本語日本', B, L, A, '日本'), solutions([(B = 0, L = 2, A = 3), (B = 3, L = 2, A = 0)])).
test(m32_ja_length_after, sub_atom('𠮷野家', B, 1, 0, S), solutions([(B = 2, S = '家')])).
test(m32_ja_before, sub_atom('𠮷野家', 1, L, A, S), solutions([
    (L = 0, A = 2, S = ''),
    (L = 1, A = 1, S = '野'),
    (L = 2, A = 0, S = '野家')
])).
test(m32_ja_scalar_sub, sub_atom('野𠮷野𠮷', B, _, A, '𠮷'), solutions([(B = 1, A = 2), (B = 3, A = 0)])).
% 日本語の名前の文法規則と、全角の文字コードの終端記号（ADR 0024）。
挨拶 --> [こんにちは], 呼び名.
呼び名 --> [太郎].
呼び名 --> [花子].
漢数字(1) --> "一".
漢数字(2) --> "二".
test(dcg_japanese, phrase(挨拶, [こんにちは, 花子]), true).
test(dcg_japanese_all, phrase(挨拶, L), solutions([L = [こんにちは, 太郎], L = [こんにちは, 花子]])).
test(dcg_japanese_codes, (atom_codes('二', Cs), phrase(漢数字(N), Cs)), solutions([N = 2])).
