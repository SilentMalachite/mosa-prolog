:- use_module(library(assoc)).
% library(assoc)（SWI-Prolog マニュアル 10.1.20 の assoc の節）。as_del_min_empty と as_del_max_empty は iso-notes の決定どおり空で失敗する。
as_double(X, Y) :- Y is X * 2.
as_put_range(From, To, Assoc0, Assoc) :-
    (   From > To
    ->  Assoc = Assoc0
    ;   put_assoc(From, Assoc0, From, Assoc1),
        Next is From + 1,
        as_put_range(Next, To, Assoc1, Assoc)
    ).
as_del_odd(From, To, Assoc0, Assoc) :-
    (   From > To
    ->  Assoc = Assoc0
    ;   del_assoc(From, Assoc0, _, Assoc1),
        Next is From + 2,
        as_del_odd(Next, To, Assoc1, Assoc)
    ).
test(as_empty, (empty_assoc(E), assoc_to_list(E, L)), solutions([L = []])).
test(as_from_list, (list_to_assoc([b-2, a-1, c-3], A), assoc_to_list(A, L)), solutions([L = [a-1, b-2, c-3]])).
test(as_from_list_dup, list_to_assoc([a-1, a-2], _), error(domain_error(unique_key_pairs, [a-1, a-2]))).
test(as_ord, (ord_list_to_assoc([a-1, b-2], O), assoc_to_keys(O, K)), solutions([K = [a, b]])).
test(as_ord_unsorted, ord_list_to_assoc([b-1, a-2], _), error(domain_error(key_ordered_pairs, [b-1, a-2]))).
test(as_ord_dup, ord_list_to_assoc([a-1, a-2], _), error(domain_error(key_ordered_pairs, [a-1, a-2]))).
test(as_get, (list_to_assoc([b-2, a-1, c-3], A), get_assoc(b, A, V)), solutions([V = 2])).
test(as_get_absent, (list_to_assoc([b-2, a-1, c-3], A), get_assoc(d, A, _)), fail).
test(as_get5, (list_to_assoc([b-2, a-1, c-3], A), (get_assoc(a, A, Old, A2, 9), assoc_to_list(A2, L))), solutions([(Old = 1, L = [a-9, b-2, c-3])])).
test(as_get5_absent, (list_to_assoc([b-2, a-1, c-3], A), get_assoc(d, A, _, _, 9)), fail).
test(as_put, (empty_assoc(E), put_assoc(k, E, 1, A1), put_assoc(k, A1, 2, A2), assoc_to_list(A2, L)), solutions([L = [k-2]])).
test(as_put_keeps_old, (list_to_assoc([b-2, a-1, c-3], A), (put_assoc(a, A, 7, _), get_assoc(a, A, V))), solutions([V = 1])).
test(as_max, (list_to_assoc([b-2, a-1, c-3], A), max_assoc(A, K, V)), solutions([(K = c, V = 3)])).
test(as_min, (list_to_assoc([b-2, a-1, c-3], A), min_assoc(A, K, V)), solutions([(K = a, V = 1)])).
test(as_max_empty, (empty_assoc(E), max_assoc(E, _, _)), fail).
test(as_gen, (list_to_assoc([b-2, a-1, c-3], A), findall(K-V, gen_assoc(K, A, V), L)), solutions([L = [a-1, b-2, c-3]])).
test(as_del, (list_to_assoc([b-2, a-1, c-3], A), (del_assoc(b, A, V, A2), assoc_to_list(A2, L))), solutions([(V = 2, L = [a-1, c-3])])).
test(as_del_absent, (list_to_assoc([b-2, a-1, c-3], A), del_assoc(d, A, _, _)), fail).
test(as_del_min, (list_to_assoc([b-2, a-1, c-3], A), (del_min_assoc(A, K, V, A2), assoc_to_list(A2, L))), solutions([(K = a, V = 1, L = [b-2, c-3])])).
test(as_del_max, (list_to_assoc([b-2, a-1, c-3], A), (del_max_assoc(A, K, V, A2), assoc_to_list(A2, L))), solutions([(K = c, V = 3, L = [a-1, b-2])])).
test(as_del_min_empty, (empty_assoc(E), del_min_assoc(E, _, _, _)), fail).
test(as_del_max_empty, (empty_assoc(E), del_max_assoc(E, _, _, _)), fail).
test(as_keys, (list_to_assoc([b-2, a-1, c-3], A), assoc_to_keys(A, K)), solutions([K = [a, b, c]])).
test(as_values, (list_to_assoc([b-2, a-1, c-3], A), assoc_to_values(A, V)), solutions([V = [1, 2, 3]])).
test(as_is, (list_to_assoc([b-2, a-1, c-3], A), is_assoc(A)), true).
test(as_is_empty, (empty_assoc(E), is_assoc(E)), true).
test(as_is_not, is_assoc(foo), fail).
test(as_balance_put, (empty_assoc(E), as_put_range(1, 1000, E, B), is_assoc(B), get_assoc(777, B, V)), solutions([V = 777])).
test(as_balance_del, (empty_assoc(E), as_put_range(1, 1000, E, B), as_del_odd(1, 999, B, C), is_assoc(C), assoc_to_keys(C, K), length(K, N)), solutions([N = 500])).
test(as_map2, (list_to_assoc([b-2, a-1, c-3], A), map_assoc(integer, A)), true).
test(as_map2_fail, (list_to_assoc([b-2, a-1, c-3], A), (put_assoc(d, A, x, A2), map_assoc(integer, A2))), fail).
test(as_map3, (list_to_assoc([b-2, a-1, c-3], A), (map_assoc(as_double, A, B), assoc_to_list(B, L))), solutions([L = [a-2, b-4, c-6]])).
