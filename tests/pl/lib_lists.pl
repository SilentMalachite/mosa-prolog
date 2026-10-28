:- use_module(library(lists)).
% 8.4.3.4 sort/2 の Examples。sort([V], V) は未定義（STO）なので写さない。
test(iso_sort_1, sort([1, 1], S), solutions([S = [1]])).
test(iso_sort_2, (sort([1+Y, z, a, V, 1, 2, V, 1, 7.0, 8.0, 1+Y, 1+2, 8.0, -a, -X, a], S), S == [V, 7.0, 8.0, 1, 2, a, z, -X, -a, 1+Y, 1+2]), true).
test(iso_sort_3, sort([1, 1], [1, 1]), fail).
test(iso_sort_4, (sort([f(U), U, U, f(V), f(U), V], L), (L == [U, V, f(U), f(V)] ; L == [V, U, f(V), f(U)])), true).
% 8.4.3.3 の誤り。
test(sort_inst, sort(_, _), error(instantiation_error)).
test(sort_partial, sort([a|_], _), error(instantiation_error)).
test(sort_type, sort(3, _), error(type_error(list, 3))).
test(sort_type_tail, sort([a|b], _), error(type_error(list, [a|b]))).
test(sort_sorted_type, sort([], 3), error(type_error(list, 3))).
test(sort_sorted_tail, sort([a, b, c], [a, b, c|d]), error(type_error(list, [a, b, c|d]))).
test(sort_sorted_partial, sort([4, 1, 3, 2, 7], [_, S|_]), solutions([S = 2])).
test(sort_vars, (sort([_, _], L), L = [A, B], var(A), var(B), A \== B), true).
% 8.4.4.4 keysort/2 の Examples。巡回する Pairs と keysort([V-V], V) は未定義（STO）なので写さない。
test(iso_keysort_1, keysort([1-1, 1-1], S), solutions([S = [1-1, 1-1]])).
test(iso_keysort_2, (keysort([2-99, 1-a, 3-f(X), 1-z, 1-a, 2-44], S), S == [1-a, 1-z, 1-a, 2-99, 2-44, 3-f(X)]), true).
test(iso_keysort_3, keysort([X-1, 1-1], [2-1, 1-1]), solutions([X = 2])).
% 8.4.4.3 の誤り。
test(keysort_inst, keysort(_, _), error(instantiation_error)).
test(keysort_partial, keysort([1-a|_], _), error(instantiation_error)).
test(keysort_type, keysort(3, _), error(type_error(list, 3))).
test(keysort_type_tail, keysort([1-a|b], _), error(type_error(list, [1-a|b]))).
test(keysort_sorted_type, keysort([], 3), error(type_error(list, 3))).
test(keysort_sorted_tail, keysort([], [1-a|b]), error(type_error(list, [1-a|b]))).
test(keysort_elem_var, keysort([_], _), error(instantiation_error)).
test(keysort_elem_pair, keysort([1/a], _), error(type_error(pair, 1/a))).
test(keysort_sorted_pair, keysort([], [1/a]), error(type_error(pair, 1/a))).
test(keysort_stable, keysort([9-8, 1-8, 9-7, 9-9, 4-1, 2-6, 1-7], S), solutions([S = [1-8, 1-7, 2-6, 4-1, 9-8, 9-7, 9-9]])).
% length/2。system にあり、lists が公開し直す。
test(length_count, length([a, b, c], N), solutions([N = 3])).
test(length_check, length([a, b], 3), fail).
test(length_neg, length(_, -1), error(domain_error(not_less_than_zero, -1))).
test(length_neg_list, length([], -1), error(domain_error(not_less_than_zero, -1))).
test(length_type, length(a, _), error(type_error(list, a))).
test(length_type_tail, length([_, _|a], 3), error(type_error(list, [_, _|a]))).
test(length_int, length([], a), error(type_error(integer, a))).
test(length_gen, (findall(L, length(L, 2), Ls), Ls = [[A, B]], var(A), var(B), A \== B), true).
test(length_partial, length([1, 2|T], 2), solutions([T = []])).
test(length_partial_short, length([1, 2, 3|_], 2), fail).
test(length_enum, findall(N, (length(_, N), (N >= 3, ! ; true)), Ns), solutions([Ns = [0, 1, 2, 3]])).
test(length_enum_tail, findall(N, (length([a|_], N), (N >= 2, ! ; true)), Ns), solutions([Ns = [1, 2]])).
test(length_qualified, lists:length([a], N), solutions([N = 1])).
% library(lists) の Prolog の述語。
test(append_join, append([a], [b, c], L), solutions([L = [a, b, c]])).
test(append_split, append(X, Y, [a, b]), solutions([(X = [], Y = [a, b]), (X = [a], Y = [b]), (X = [a, b], Y = [])])).
test(member_enum, member(X, [a, b]), solutions([X = a, X = b])).
test(member_none, member(c, [a, b]), fail).
test(memberchk_once, memberchk(X, [a, b]), solutions([X = a])).
test(nth0_index, nth0(1, [a, b, c], E), solutions([E = b])).
test(nth0_enum, nth0(I, [a, b], E), solutions([(I = 0, E = a), (I = 1, E = b)])).
test(nth0_out, nth0(3, [a, b, c], _), fail).
test(nth0_type, nth0(a, [a], _), error(type_error(integer, a))).
test(nth1_index, nth1(1, [a, b, c], E), solutions([E = a])).
test(nth1_enum, nth1(I, [a, b], E), solutions([(I = 1, E = a), (I = 2, E = b)])).
test(nth1_zero, nth1(0, [a], _), fail).
test(last_elem, last([a, b, c], X), solutions([X = c])).
test(last_empty, last([], _), fail).
test(reverse_list, reverse([a, b, c], L), solutions([L = [c, b, a]])).
test(msort_dups, msort([b, a, c, a], L), solutions([L = [a, a, b, c]])).
test(msort_order, (msort([1+Y, z, a, V, 1, 2, V, 1, 7.0, 8.0, 1+Y, 1+2, 8.0, -a, -X, a], S), S == [V, V, 7.0, 8.0, 8.0, 1, 1, 2, a, a, z, -X, -a, 1+Y, 1+Y, 1+2]), true).
test(msort_inst, msort(_, _), error(instantiation_error)).
test(msort_partial, msort([a|_], _), error(instantiation_error)).
test(msort_type, msort([a|b], _), error(type_error(list, [a|b]))).
test(msort_sorted_type, msort([a, b, a], [a, a, b|d]), error(type_error(list, [a, a, b|d]))).
test(msort_fail, msort([2], [3]), fail).
test(sum_list_ints, sum_list([1, 2, 3], S), solutions([S = 6])).
test(sum_list_float, sum_list([1, 2.5], S), solutions([S = 3.5])).
test(sum_list_empty, sum_list([], S), solutions([S = 0])).
test(max_list_ints, max_list([3, 1, 4, 1], M), solutions([M = 4])).
test(max_list_empty, max_list([], _), fail).
test(list_to_set_dups, list_to_set([a, b, a, c, b], S), solutions([S = [a, b, c]])).
test(list_to_set_eq, (list_to_set([X, Y, X], S), S == [X, Y]), true).
test(list_to_set_partial, list_to_set([a|_], _), error(instantiation_error)).
test(list_to_set_type, list_to_set([a|b], _), error(type_error(list, [a|b]))).
% 巡回するリストは type_error(list, _)。Formal を比べると巡回項を辿るので、catch で形だけ見る。
test(length_cyclic, (L = [a, b|L], catch(length(L, _), error(type_error(list, _), _), true)), true).
test(sort_cyclic, (L = [3, 1, 2|L], catch(sort(L, _), error(type_error(list, _), _), true)), true).
test(msort_cyclic, (L = [3, 1, 2|L], catch(msort(L, _), error(type_error(list, _), _), true)), true).
