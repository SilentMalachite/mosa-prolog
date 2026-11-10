% 文法規則（-->）と phrase/2,3。ADR 0024。13211-3 の本文は手元に無い（iso-notes「13211-3」）ので、本文の
% Examples が手に入るまでは自前のテストである。エラーは Formal だけを比べる。

greeting --> [hello], dcg_name.
dcg_name --> [world].
dcg_name --> [prolog].

test(dcg_phrase2, phrase(greeting, [hello, world]), true).
test(dcg_phrase2_other, phrase(greeting, [hello, prolog]), true).
test(dcg_phrase2_short, phrase(greeting, [hello]), fail).
test(dcg_phrase2_long, phrase(greeting, [hello, world, x]), fail).
test(dcg_phrase3_rest, phrase(greeting, [hello, world, x], R), solutions([R = [x]])).
test(dcg_phrase_enumerate, phrase(greeting, L), solutions([L = [hello, world], L = [hello, prolog]])).
test(dcg_phrase_partial, phrase(greeting, [hello|T]), solutions([T = [world], T = [prolog]])).

% 本体の形ごとに 1 件以上。
dcg_list --> [a, b].
dcg_empty --> [].
dcg_or --> [a] ; [b].
dcg_bar --> '|'([a], [b]).
dcg_ite(X) --> ( [a] -> { X = yes } ; { X = no } ).
dcg_it --> ( [a] -> [b] ).
dcg_not --> \+ [a], [_].
dcg_brace(X) --> [N], { X is N * 2 }.
dcg_call --> call(dcg_tail, x).
dcg_tail(X, S0, S) :- S0 = [X|S].
dcg_var(G) --> G.
dcg_codes --> "ab".
dcg_mod --> user:dcg_name.
dcg_args(X, Y) --> [X], [Y].

test(dcg_list, phrase(dcg_list, [a, b]), true).
test(dcg_list_rest, phrase(dcg_list, [a, b, c], R), solutions([R = [c]])).
test(dcg_empty, phrase(dcg_empty, []), true).
test(dcg_empty_rest, phrase(dcg_empty, [a], R), solutions([R = [a]])).
test(dcg_or, phrase(dcg_or, L), solutions([L = [a], L = [b]])).
test(dcg_bar, phrase(dcg_bar, L), solutions([L = [a], L = [b]])).
test(dcg_ite_then, phrase(dcg_ite(X), [a]), solutions([X = yes])).
test(dcg_ite_else, phrase(dcg_ite(X), [b], R), solutions([(X = no, R = [b])])).
test(dcg_it, phrase(dcg_it, [a, b]), true).
test(dcg_it_fail, phrase(dcg_it, [b]), fail).
test(dcg_not_ok, phrase(dcg_not, [b]), true).
test(dcg_not_fail, phrase(dcg_not, [a]), fail).
test(dcg_not_consumes_none, phrase(\+ [a], [b], R), solutions([R = [b]])).
test(dcg_brace, phrase(dcg_brace(X), [3]), solutions([X = 6])).
test(dcg_brace_consumes_none, phrase({ true }, [a], R), solutions([R = [a]])).
test(dcg_call, phrase(dcg_call, [x]), true).
test(dcg_var_list, phrase(dcg_var([a]), [a]), true).
test(dcg_var_nonterminal, phrase(dcg_var(greeting), [hello, world]), true).
test(dcg_codes, phrase(dcg_codes, [0'a, 0'b]), true).
test(dcg_mod, phrase(dcg_mod, [world]), true).
test(dcg_args, phrase(dcg_args(X, Y), [1, 2]), solutions([(X = 1, Y = 2)])).

% 頭部のプッシュバック。本体の後で残りの前に足す。
dcg_push, [x] --> [a].
dcg_push2, [x, y] --> [].
test(dcg_push, phrase(dcg_push, [a], R), solutions([R = [x]])).
test(dcg_push_rest, phrase(dcg_push, [a, b], R), solutions([R = [x, b]])).
test(dcg_push_two, phrase(dcg_push2, [b], R), solutions([R = [x, y, b]])).

% カットは節の中に閉じる。終端記号を頭部で単一化しないので、終端記号の前のカットも節を選んでから切る（ADR 0024）。
dcg_cut(a) --> [a], !.
dcg_cut(other) --> [_].
dcg_cut_first(X) --> !, [X].
dcg_cut_first(none) --> [].
test(dcg_cut, phrase(dcg_cut(X), [a]), solutions([X = a])).
test(dcg_cut_other, phrase(dcg_cut(X), [b]), solutions([X = other])).
test(dcg_cut_first, phrase(dcg_cut_first(X), []), fail).
test(dcg_cut_first_item, phrase(dcg_cut_first(X), [q]), solutions([X = q])).

% phrase/2,3 の本体は文法規則の本体として訳す。中のカットは phrase に閉じる（call/1 と同じ）。
dcg_cut_local(X) :- ( X = 1 ; X = 2 ), phrase(!, [], []).
test(dcg_phrase_list, phrase([a, b], L), solutions([L = [a, b]])).
test(dcg_phrase_empty, phrase([], L), solutions([L = []])).
test(dcg_phrase_conj, phrase(([a], [b]), L), solutions([L = [a, b]])).
test(dcg_phrase_or, phrase(([a] ; [b]), L), solutions([L = [a], L = [b]])).
test(dcg_phrase_cut_local, dcg_cut_local(X), solutions([X = 1, X = 2])).
test(dcg_phrase_cut_inside, phrase((!, [a] ; [b]), L), solutions([L = [a]])).
test(dcg_phrase_call, phrase(call(dcg_tail, y), [y]), true).
test(dcg_phrase_qualified, phrase(user:greeting, [hello, world]), true).

% phrase/2,3 のエラー。
test(dcg_phrase_var, phrase(_, []), error(instantiation_error)).
test(dcg_phrase3_var, phrase(_, [], _), error(instantiation_error)).
test(dcg_phrase_number, phrase(1, []), error(type_error(callable, 1))).
test(dcg_phrase_inner_number, phrase((greeting, 1), []), error(type_error(callable, (greeting, 1)))).
test(dcg_phrase_list_atom, phrase(greeting, a), error(type_error(list, a))).
test(dcg_phrase_list_bad_tail, phrase(greeting, [hello|world]), error(type_error(list, [hello|world]))).
test(dcg_phrase_rest_atom, phrase(greeting, [hello, world], b), error(type_error(list, b))).
test(dcg_phrase_qualified_var, phrase(user:_, []), error(instantiation_error)).

% 読み込み時の誤り。どれも読み込みを失敗にする（Formal は tests/cpp/dcg_test.cpp が診断の文で確かめる）。
dcg_load(Name, Text) :-
    atom_concat('tests/tmp/dcg_', Name, Base), atom_concat(Base, '.pl', Path),
    open(Path, write, S), write(S, Text), close(S), consult(Path).
test(dcg_load_ok, (dcg_load(ok, 'dcg_ok --> [a].\n'), phrase(dcg_ok, [a])), true).
test(dcg_load_var_head, dcg_load(var_head, 'X --> [a].\n'), fail).
test(dcg_load_number_head, dcg_load(number_head, '1 --> [a].\n'), fail).
test(dcg_load_number_body, dcg_load(number_body, 'dcg_nb --> [a], 1.\n'), fail).
test(dcg_load_bad_list, dcg_load(bad_list, 'dcg_bl --> [a|b].\n'), fail).
test(dcg_load_bad_push, dcg_load(bad_push, 'dcg_bp, x --> [a].\n'), fail).
test(dcg_load_qualified, dcg_load(qualified, 'user:(dcg_q --> [a]).\n'), fail).
