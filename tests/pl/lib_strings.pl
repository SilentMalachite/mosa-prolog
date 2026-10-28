:- use_module(library(strings)).
% 誤りの形は Logtalk の atomic_list_concat_2、atomic_list_concat_3 と同じ。split_atom/4 は SWI の split_string/4 の切り方。
% atomic_list_concat/2。
test(alc_join, atomic_list_concat([a, 1, 'あ'], X), solutions([X = 'a1あ'])).
test(alc_empty, atomic_list_concat([], X), solutions([X = ''])).
test(alc_numbers, atomic_list_concat([a, 1, b, '', +, 2.0, [], c, -3.5], X), solutions([X = 'a1b+2.0[]c-3.5'])).
test(alc_curly, atomic_list_concat([{}, x], X), solutions([X = '{}x'])).
test(alc_det, findall(X, atomic_list_concat([a, b], X), L), solutions([L = [ab]])).
test(alc_check, atomic_list_concat([a, b], ab), true).
test(alc_check_fail, atomic_list_concat([a, b], ba), fail).
test(alc_inst, atomic_list_concat([a, _], _), error(instantiation_error)).
test(alc_inst_bound, atomic_list_concat([_, bar], foobar), error(instantiation_error)).
test(alc_inst_tail, atomic_list_concat([foo, bar|_], foobar), error(instantiation_error)).
test(alc_inst_list, atomic_list_concat(_, foobar), error(instantiation_error)).
test(alc_type_list, atomic_list_concat(foo, _), error(type_error(list, foo))).
test(alc_type_elem, atomic_list_concat([a(1), bar], _), error(type_error(atomic, a(1)))).
test(alc_type_out, atomic_list_concat([foo, bar], a(1)), error(type_error(atom, a(1)))).
test(alc_type_out_num, atomic_list_concat([1, 2], 12), error(type_error(atom, 12))).
% atomic_list_concat/3 で繋ぐ。
test(alc_sep, atomic_list_concat([a, b], '、', X), solutions([X = 'a、b'])).
test(alc_sep_three, atomic_list_concat([a, 42, c], '_', X), solutions([X = a_42_c])).
test(alc_sep_one, atomic_list_concat([a], '_', X), solutions([X = a])).
test(alc_sep_empty_list, atomic_list_concat([], '-', X), solutions([X = ''])).
test(alc_sep_number, atomic_list_concat([a, b], 0, X), solutions([X = a0b])).
test(alc_sep_det, findall(X, atomic_list_concat([a, b], '_', X), L), solutions([L = [a_b]])).
test(alc_sep_inst_head, atomic_list_concat([_, bar], '_', _), error(instantiation_error)).
test(alc_sep_inst_tail, atomic_list_concat([foo, bar|_], '_', _), error(instantiation_error)).
test(alc_sep_inst_sep, atomic_list_concat([foo, bar], _, _), error(instantiation_error)).
test(alc_sep_type_elem, atomic_list_concat([a(1), bar], '_', _), error(type_error(atomic, a(1)))).
test(alc_sep_type_sep, atomic_list_concat([foo, bar], a(1), _), error(type_error(atomic, a(1)))).
test(alc_sep_type_out, atomic_list_concat([foo, bar], '_', a(1)), error(type_error(atom, a(1)))).
% atomic_list_concat/3 で分ける。リストが決まっていず、原子が決まっているとき。
test(alc_split, atomic_list_concat(L, ',', 'a,b'), solutions([L = [a, b]])).
test(alc_split_multi, atomic_list_concat(L, ', ', 'a, b, c'), solutions([L = [a, b, c]])).
test(alc_split_edges, atomic_list_concat(L, ',', ',a,,b,'), solutions([L = ['', a, '', b, '']])).
test(alc_split_none, atomic_list_concat(L, ',', abc), solutions([L = [abc]])).
test(alc_split_empty, atomic_list_concat(L, ',', ''), solutions([L = ['']])).
test(alc_split_kana, atomic_list_concat(L, 'の', '東京の空'), solutions([L = ['東京', '空']])).
test(alc_split_partial, atomic_list_concat([a|T], '-', 'a-b-c'), solutions([T = [b, c]])).
test(alc_split_digits, atomic_list_concat(L, '.', '1.2'), solutions([L = ['1', '2']])).
test(alc_split_det, findall(L, atomic_list_concat(L, ',', 'a,b'), Ls), solutions([Ls = [[a, b]]])).
test(alc_split_empty_sep, atomic_list_concat(_, '', abc), error(domain_error(non_empty_atom, ''))).
test(alc_split_type_list, atomic_list_concat(foo, ',', 'a,b'), error(type_error(list, foo))).
% split_atom/4。
test(split_atom_basic, split_atom('a b  c', ' ', '', L), solutions([L = [a, b, '', c]])).
test(split_atom_pad, split_atom(' a ', '', ' ', L), solutions([L = [a]])).
test(split_atom_dots, split_atom('a.b.c.d', '.', '', L), solutions([L = [a, b, c, d]])).
test(split_atom_edges, split_atom('.a.b.', '.', '', L), solutions([L = ['', a, b, '']])).
test(split_atom_collapse, split_atom('/home//jan///nice/path', '/', '/', L), solutions([L = [home, jan, nice, path]])).
test(split_atom_collapse_edges, split_atom('.a.b...c.', '.', '.', L), solutions([L = [a, b, c]])).
test(split_atom_trim, split_atom('SWI-Prolog, 7.0', ',', ' ', L), solutions([L = ['SWI-Prolog', '7.0']])).
test(split_atom_trim_inner, split_atom('  SWI Prolog  ', '', ' ', L), solutions([L = ['SWI Prolog']])).
test(split_atom_any_sep, split_atom('a,b;c', ',;', '', L), solutions([L = [a, b, c]])).
test(split_atom_empty, split_atom('', ',', '', L), solutions([L = ['']])).
test(split_atom_all_pad, split_atom('   ', '', ' ', L), solutions([L = ['']])).
test(split_atom_trailing_sep, split_atom('a,', ',', '', L), solutions([L = [a, '']])).
test(split_atom_kanji, split_atom('東京、大阪 、 京都', '、', ' ', L), solutions([L = ['東京', '大阪', '京都']])).
test(split_atom_number, split_atom(12.5, '.', '', L), solutions([L = ['12', '5']])).
test(split_atom_det, findall(L, split_atom('a b', ' ', '', L), Ls), solutions([Ls = [[a, b]]])).
test(split_atom_inst, split_atom(_, ' ', '', _), error(instantiation_error)).
test(split_atom_inst_sep, split_atom(a, _, '', _), error(instantiation_error)).
test(split_atom_inst_pad, split_atom(a, ' ', _, _), error(instantiation_error)).
test(split_atom_type, split_atom(f(x), ' ', '', _), error(type_error(atomic, f(x)))).
test(split_atom_type_sep, split_atom(a, [x], '', _), error(type_error(atomic, [x]))).
% upcase_atom/2。単純大文字化なので 1 文字は 1 文字のまま。入力は原子か数、結果は変数か原子（atomic_list_concat と同じ）。
test(upcase_ascii, upcase_atom(abc, X), solutions([X = 'ABC'])).
test(upcase_check, upcase_atom(abc, 'ABC'), true).
test(upcase_check_fail, upcase_atom(abc, abc), fail).
test(upcase_greek, upcase_atom('σ', X), solutions([X = 'Σ'])).
test(upcase_kana_kept, upcase_atom('あa', X), solutions([X = 'あA'])).
test(upcase_sharp_s, upcase_atom('ß', X), solutions([X = 'ß'])).
test(upcase_fullwidth, upcase_atom('ａｂ', X), solutions([X = 'ＡＢ'])).
test(upcase_astral, upcase_atom('𐐨', X), solutions([X = '𐐀'])).
test(upcase_empty, upcase_atom('', X), solutions([X = ''])).
test(upcase_mixed, upcase_atom('Hello World', X), solutions([X = 'HELLO WORLD'])).
test(upcase_number, upcase_atom(1.0e10, X), solutions([X = '1.0E+10'])).
test(upcase_det, findall(X, upcase_atom(ab, X), L), solutions([L = ['AB']])).
test(upcase_inst, upcase_atom(_, _), error(instantiation_error)).
test(upcase_type, upcase_atom(f(x), _), error(type_error(atomic, f(x)))).
test(upcase_type_out, upcase_atom(a, f(x)), error(type_error(atom, f(x)))).
test(upcase_type_out_num, upcase_atom(a, 1), error(type_error(atom, 1))).
% '$upcase_atom'/2 は system にある。
test(upcase_raw, '$upcase_atom'(xyz, X), solutions([X = 'XYZ'])).
test(upcase_raw_inst, '$upcase_atom'(_, _), error(instantiation_error)).
test(upcase_raw_type, '$upcase_atom'(1, _), error(type_error(atom, 1))).
