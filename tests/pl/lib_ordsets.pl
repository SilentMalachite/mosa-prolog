:- use_module(library(ordsets)).
% library(ordsets)（SWI-Prolog マニュアル 10.1.20 の ordsets の節）。os_symdiff_doc はそのマニュアルの例。
test(os_is, is_ordset([a, b, c]), true).
test(os_is_unsorted, is_ordset([b, a]), fail).
test(os_is_dup, is_ordset([a, a]), fail).
test(os_is_term, is_ordset(foo), fail).
test(os_empty, ord_empty([]), true).
test(os_empty_not, ord_empty([a]), fail).
test(os_seteq, ord_seteq([a, b], [a, b]), true).
test(os_seteq_not, ord_seteq([a], [b]), fail).
test(os_from_list, list_to_ord_set([c, a, b, a], S), solutions([S = [a, b, c]])).
test(os_intersect, ord_intersect([a, b], [b, c]), true).
test(os_intersect_not, ord_intersect([a], [b]), fail).
test(os_disjoint, ord_disjoint([a], [b]), true).
test(os_disjoint_not, ord_disjoint([a, b], [b]), fail).
test(os_inter2, ord_intersection([[a, b, c], [b, c, d], [b, c]], I), solutions([I = [b, c]])).
test(os_inter2_empty, ord_intersection([], _), fail).
test(os_inter3, ord_intersection([a, b, c], [b, c, d], I), solutions([I = [b, c]])).
test(os_inter4, ord_intersection([a, b, c], [b, c, d], I, D), solutions([(I = [b, c], D = [d])])).
test(os_add, ord_add_element([a, c], b, S), solutions([S = [a, b, c]])).
test(os_add_present, ord_add_element([a, c], c, S), solutions([S = [a, c]])).
test(os_del, ord_del_element([a, b, c], b, S), solutions([S = [a, c]])).
test(os_del_absent, ord_del_element([a, c], b, S), solutions([S = [a, c]])).
test(os_selectchk, ord_selectchk(b, [a, b, c], R), solutions([R = [a, c]])).
test(os_selectchk_absent, ord_selectchk(d, [a, b, c], _), fail).
% Set1 が変数なら、Item を Set2 に差し込んで Set1 を作る（マニュアルの ord_selectchk(+Item, ?Set1, ?Set2)）。
test(os_selectchk_build, ord_selectchk(b, S, [a, c]), solutions([S = [a, b, c]])).
test(os_selectchk_build_end, ord_selectchk(d, S, [a, c]), solutions([S = [a, c, d]])).
test(os_selectchk_build_present, ord_selectchk(a, _, [a, c]), fail).
test(os_memberchk, ord_memberchk(b, [a, b, c]), true).
test(os_memberchk_not, ord_memberchk(d, [a, b, c]), fail).
test(os_range, ord_range([1, 3, 5, 7], 2, 6, R), solutions([R = [3, 5]])).
test(os_range_ends, ord_range([1, 3, 5], 3, 5, R), solutions([R = [3, 5]])).
test(os_subset, ord_subset([a, c], [a, b, c]), true).
test(os_subset_not, ord_subset([d], [a]), fail).
test(os_subtract, ord_subtract([a, b, c], [b], D), solutions([D = [a, c]])).
test(os_union2, ord_union([[c], [a, b], [b, d]], U), solutions([U = [a, b, c, d]])).
test(os_union3, ord_union([a, c], [b, c], U), solutions([U = [a, b, c]])).
test(os_union4, ord_union([a, c], [b, c], U, N), solutions([(U = [a, b, c], N = [b])])).
test(os_symdiff_doc, ord_symdiff([1, 2], [2, 3], X), solutions([X = [1, 3]])).
