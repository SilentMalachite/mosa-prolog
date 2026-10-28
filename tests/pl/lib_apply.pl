:- use_module(library(apply)).
:- use_module(library(lists)).
% 閉包に渡す述語。user にあり、meta_predicate の限定で library(apply) から呼べる。
plus3(X, V0, V) :- V is X + V0.
sum4(A, B, C, D) :- D is A + B + C.
sum5(A, B, C, V0, V) :- V is A + B + C + V0.
% maplist/2..5。
test(maplist2_all, maplist(integer, [1, 2]), true).
test(maplist2_one_fails, maplist(integer, [1, a]), fail).
test(maplist2_empty, maplist(integer, []), true).
test(maplist2_fill, (length(L, 2), maplist(=(x), L)), solutions([L = [x, x]])).
test(maplist3_map, maplist(=, [a, b], L), solutions([L = [a, b]])).
test(maplist_len_diff, maplist(=, [a], [a, b]), fail).
test(maplist4_map, maplist(plus3, [1, 2], [10, 20], L), solutions([L = [11, 22]])).
test(maplist5_map, maplist(sum4, [1, 2], [10, 20], [100, 200], L), solutions([L = [111, 222]])).
test(maplist_backtrack, maplist(member, [X, Y], [[a, b], [c]]), solutions([(X = a, Y = c), (X = b, Y = c)])).
test(maplist_inst, maplist(_, [a]), error(instantiation_error)).
test(maplist_closure_type, maplist(1, [a]), error(type_error(callable, _))).
% foldl/4..6。
test(foldl_sum, foldl(plus3, [1, 2, 3], 0, S), solutions([S = 6])).
test(foldl_empty, foldl(plus3, [], a, S), solutions([S = a])).
test(foldl5_sum, foldl(sum4, [1, 2], [10, 20], 0, S), solutions([S = 33])).
test(foldl5_len_diff, foldl(sum4, [1], [1, 2], 0, _), fail).
test(foldl6_sum, foldl(sum5, [1], [2], [3], 10, S), solutions([S = 16])).
% include/3、exclude/3。
test(include_ints, include(integer, [a, 1, b, 2], L), solutions([L = [1, 2]])).
test(include_none, include(integer, [a, b], L), solutions([L = []])).
test(exclude_ints, exclude(integer, [a, 1, b, 2], L), solutions([L = [a, b]])).
test(exclude_all, exclude(atom, [a, b], L), solutions([L = []])).
test(include_once, include(member(a), [[a, a], [b], [a]], L), solutions([L = [[a, a], [a]]])).
% 宣言した meta_predicate が取り込み側から見える。
test(pp_maplist_meta, predicate_property(maplist(_, _), meta_predicate(M)), solutions([M = maplist(1, ?)])).
test(pp_foldl_meta, predicate_property(foldl(_, _, _, _), meta_predicate(M)), solutions([M = foldl(3, ?, ?, ?)])).
