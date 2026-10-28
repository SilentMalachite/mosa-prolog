use_lists :- use_module(library(lists)).

% 非公開の補助述語は、定義したモジュールの節から呼べる。
test(lists_reverse, (use_lists, reverse([a, b, c], [c, b, a])), true).
test(lists_append, (use_lists, append([a], [b, c], [a, b, c])), true).
test(lists_append_split, (use_lists, append(Left, Right, [a, b])), solutions([
  (Left = [], Right = [a, b]),
  (Left = [a], Right = [b]),
  (Left = [a, b], Right = [])
])).
% memberchk は最初の解で切る。
test(lists_memberchk, (use_lists, findall(X, memberchk(X, [a, a]), Found), Found = [a]), true).
test(lists_memberchk_miss, (use_lists, memberchk(c, [a, b])), fail).
test(lists_nth0, (use_lists, nth0(1, [a, b, c], b)), true).
test(lists_nth0_gen, (use_lists, nth0(N, [a, b], E)), solutions([(N = 0, E = a), (N = 1, E = b)])).
test(lists_nth1, (use_lists, nth1(2, [a, b, c], b)), true).
test(lists_nth1_gen, (use_lists, nth1(N, [a, b], E)), solutions([(N = 1, E = a), (N = 2, E = b)])).
test(lists_last, (use_lists, last([a, b, c], c)), true).
% 重複を残し、標準順で並べる。
test(lists_msort, (use_lists, msort([b, a, b], [a, b, b])), true).
test(lists_msort_empty, (use_lists, msort([], [])), true).
test(lists_sum, (use_lists, sum_list([1, 2, 3], 6)), true).
test(lists_sum_empty, (use_lists, sum_list([], 0)), true).
test(lists_max, (use_lists, max_list([1, 3, 2], 3)), true).
test(lists_max_empty, (use_lists, max_list([], _)), fail).
test(lists_set, (use_lists, list_to_set([a, b, a, c], [a, b, c])), true).

% レビューで直した不具合の回帰。
% 降順の 400 要素でもヒープの上限に届かない。
rv_down(0, []) :- !.
rv_down(N, [N|T]) :- M is N - 1, rv_down(M, T).
test(lists_msort_long, (use_lists, rv_down(400, L), msort(L, S), S = [1, 2|_], last(S, 400)), true).
test(lists_msort_mismatch, (use_lists, msort([b, a], [b, a])), fail).
test(lists_msort_dups, (use_lists, msort([c, a, b, a, c], S), S == [a, a, b, c, c]), true).
% 変数を含む要素は == で比べる。束縛しない。
test(lists_set_vars, (use_lists, list_to_set([A, B, A], S), S == [A, B], var(A), var(B), A \== B), true).
% 整数の添字は開いたリストを列挙し続けない。
test(lists_nth0_open, (use_lists, nth0(1, L, x), L = [_, x|_]), true).
test(lists_nth0_open_fail, (use_lists, nth0(1, [_, y|_], x)), fail).
test(lists_nth1_zero, (use_lists, nth1(0, _, _)), fail).
test(lists_nth0_type, (use_lists, nth0(a, [x], _)), error(type_error(integer, a))).
