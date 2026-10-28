use_strings :- use_module(library(strings)).

% 取り込むまで user からは見えない。
test(strings_hidden, atomic_list_concat([a, b], _), error(existence_error(procedure, atomic_list_concat/2))).
test(strings_concat3_hidden, atomic_list_concat([a], '-', _), error(existence_error(procedure, atomic_list_concat/3))).
test(strings_split_hidden, split_atom('a.b', '.', '', _), error(existence_error(procedure, split_atom/4))).
test(strings_upcase_hidden, upcase_atom(ab, _), error(existence_error(procedure, upcase_atom/2))).

% アトムの並びを区切りなしで1つのアトムにする。
test(strings_concat, (use_strings, atomic_list_concat([a, b], A), A == ab), true).

% 区切りアトムを要素のあいだに挟む。
test(strings_sep, (use_strings, atomic_list_concat([gnu, gnat], ', ', A), A == 'gnu, gnat'), true).

% 整数は十進の表記で挟む。
test(strings_integer, (use_strings, atomic_list_concat([a, 42, c], '_', A), A == 'a_42_c'), true).

% 空アトム、空リスト、小数も字面のまま繋ぐ。
test(strings_atomics, (use_strings, atomic_list_concat([a, 1, b, '', +, 2.0, [], c, -3.5], A), A == 'a1b+2.0[]c-3.5'), true).

% 区切りとアトムが具体値で、リストが変数なら分ける。
test(strings_split, (use_strings, atomic_list_concat(L, '-', 'gnu-gnat'), L == [gnu, gnat]), true).

% 隣り合う区切りは空アトムを残す。
test(strings_split_empty_part, (use_strings, atomic_list_concat(L, '&', 'a&&b&&c'), L == [a, '', b, '', c]), true).

% 端の区切りも空アトムを残す。
test(strings_split_edges, (use_strings, atomic_list_concat(L, '&', '&a&&b&&c&'), L == ['', a, '', b, '', c, '']), true).

% 区切りが2文字以上のときは、その並びごとで分ける。
test(strings_split_long, (use_strings, atomic_list_concat(L, '::', 'a::b::c'), L == [a, b, c]), true).

% 繋ぐ側は空の区切りを挟める。
test(strings_join_empty, (use_strings, atomic_list_concat([fo, o], '', A), A == foo), true).

% 区切り文字のたびに分ける。埋めが空なら空の部分も残す。
test(strings_chars, (use_strings, split_atom('a.b.c.d', '.', '', L), L == [a, b, c, d]), true).

% 部分の前後にある埋め文字は捨てる。
test(strings_pad, (use_strings, split_atom('SWI-Prolog, 7.0', ',', ' ', L), L == ['SWI-Prolog', '7.0']), true).

% 区切りと埋めが同じなら、連続した区切りは1つに畳む。
test(strings_pad_sep, (use_strings, split_atom('/home//jan///nice/path', '/', '/', L), L == [home, jan, nice, path]), true).

% 区切りが空なら、前後の埋めを落として1つにする。
test(strings_trim, (use_strings, split_atom('  SWI-Prolog  ', '', ' ', L), L == ['SWI-Prolog']), true).

% 埋めが空なら、連続した区切りは空アトムを残す。
test(strings_gap, (use_strings, split_atom('a..b', '.', '', L), L == [a, '', b]), true).

% 単純大文字写像。ASCII は a..z を A..Z にする。
test(strings_upcase, (use_strings, upcase_atom(ab, A), A == 'AB'), true).

% 単純大文字写像は符号位置1つを符号位置1つへ写す。
test(strings_upcase_accent, (use_strings, upcase_atom('é', A), A == 'É'), true).

% 単純大文字写像が無い符号位置はそのまま。ß は SS にしない。
test(strings_upcase_eszett, (use_strings, upcase_atom('ß', A), A == 'ß'), true).

% 語末シグマは Σ。差は -32 ではない。
test(strings_upcase_sigma, (use_strings, upcase_atom('ς', A), A == 'Σ'), true).

% 全角のラテン小文字も単純大文字写像で全角の大文字になる。
test(strings_upcase_wide, (use_strings, upcase_atom('ａ', A), A == 'Ａ'), true).

test(strings_upcase_var, (use_strings, upcase_atom(_, _)), error(instantiation_error)).
test(strings_upcase_type, (use_strings, upcase_atom(42, _)), error(type_error(atom, 42))).

% 空の区切りでは分けない。繋ぐ側は空の区切りを許す。
test(strings_split_empty, (use_strings, atomic_list_concat(_, '', foo)), error(domain_error(non_empty_atom, ''))).

% 要素が変数なら、結果を見る前に instantiation_error。
test(strings_var_elem, (use_strings, atomic_list_concat([_, bar], foobar)), error(instantiation_error)).

% リストの末尾が変数でも instantiation_error。
test(strings_var_tail, (use_strings, atomic_list_concat([foo, bar|_], foobar)), error(instantiation_error)).

% 要素がアトミックでなければ type_error(atomic)。
test(strings_elem_type, (use_strings, atomic_list_concat([a(1), bar], foobar)), error(type_error(atomic, a(1)))).

% 区切りが変数なら instantiation_error。
test(strings_sep_var, (use_strings, atomic_list_concat([foo, bar], _, _)), error(instantiation_error)).

% 区切りがアトミックでなければ type_error(atomic)。
test(strings_sep_type, (use_strings, atomic_list_concat([foo, bar], a(1), _)), error(type_error(atomic, a(1)))).

% 要素が1つでも、区切りの型は見る。
test(strings_sep_one, (use_strings, atomic_list_concat([a], a(1), _)), error(type_error(atomic, a(1)))).

% 結果がアトムでなければ type_error(atom)。
test(strings_result_type, (use_strings, atomic_list_concat([foo, bar], '_', a(1))), error(type_error(atom, a(1)))).

% リストでなければ type_error(list)。
test(strings_list_type, (use_strings, atomic_list_concat(foo, _)), error(type_error(list, foo))).

% レビューで直した不具合の回帰。結果が合わないときと、やり直しでは失敗する。例外は投げない。
test(strings_concat_mismatch, (use_strings, atomic_list_concat([a, b], '-', foo)), fail).
test(strings_concat_empty_mismatch, (use_strings, atomic_list_concat([], '-', foo)), fail).
test(strings_concat_redo, (use_strings, atomic_list_concat([a, b], '-', _), fail), fail).
test(strings_concat2_redo, (use_strings, findall(A, ((X = a ; X = b), atomic_list_concat([X, y], A)), L), L == [ay, by]), true).
