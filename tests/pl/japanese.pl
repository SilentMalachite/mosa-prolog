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
