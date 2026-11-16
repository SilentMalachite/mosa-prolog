:- use_module(library(pairs)).
% library(pairs)（SWI-Prolog マニュアル 10.1.20 の pairs の節）。group_pairs_by_key の 2 つの例はそのマニュアルの例。
pair_key(X, k(X)).
test(pkv_split, pairs_keys_values([a-1, b-2], K, V), solutions([(K = [a, b], V = [1, 2])])).
test(pkv_join, pairs_keys_values(P, [a, b], [1, 2]), solutions([P = [a-1, b-2]])).
test(pkv_empty, pairs_keys_values([], K, V), solutions([(K = [], V = [])])).
test(pk_keys, pairs_keys([a-1, b-2], K), solutions([K = [a, b]])).
test(pv_values, pairs_values([a-1, b-2], V), solutions([V = [1, 2]])).
test(gpk_doc, group_pairs_by_key([a-2, a-1, b-4, a-3], X), solutions([X = [a-[2, 1], b-[4], a-[3]]])).
test(gpk_doc_largest, (sort(1, @>=, [a-1, b-2, c-3, a-4, a-5, c-6], Ps), group_pairs_by_key(Ps, [K-Vs|_])), solutions([(K = c, Vs = [3, 6])])).
test(gpk_strict, group_pairs_by_key([1-a, 1.0-b], G), solutions([G = [1-[a], 1.0-[b]]])).
test(gpk_empty, group_pairs_by_key([], G), solutions([G = []])).
test(tp_sorted, transpose_pairs([a-2, b-1, c-2], T), solutions([T = [1-b, 2-a, 2-c]])).
test(mlp_length, map_list_to_pairs(atom_length, [abc, d], P), solutions([P = [3-abc, 1-d]])).
test(mlp_user, map_list_to_pairs(pair_key, [a], P), solutions([P = [k(a)-a]])).
% setup_call_cleanup/3 の後始末は、ゴールが選択点を残さずに成功したときだけ、その直後に走る（atoms.pl の m32_det と同じ）。
pr_det(Goal) :- setup_call_cleanup(true, Goal, Det = yes), Det == yes.
test(pkv_split_det, (pr_det(pairs_keys_values([a-1, b-2], K, V)), K == [a, b], V == [1, 2]), true).
test(pkv_join_det, (pr_det(pairs_keys_values(P, [a, b], [1, 2])), P == [a-1, b-2]), true).
test(pk_keys_det, (pr_det(pairs_keys([a-1, b-2], K)), K == [a, b]), true).
test(pv_values_det, (pr_det(pairs_values([a-1, b-2], V)), V == [1, 2]), true).
test(tp_det, (pr_det(transpose_pairs([a-2, b-1, c-2], T)), T == [1-b, 2-a, 2-c]), true).
test(gpk_det, (pr_det(group_pairs_by_key([a-2, a-1, b-4, a-3], G)), G == [a-[2, 1], b-[4], a-[3]]), true).
test(mlp_det, (pr_det(map_list_to_pairs(atom_length, [abc, d], P)), P == [3-abc, 1-d]), true).
