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
% setup_call_cleanup/3 の後始末は、ゴールが選択点を残さずに成功したときだけ、その直後に走る（atoms.pl の m32_det と同じ）。
os_det(Goal) :- setup_call_cleanup(true, Goal, Det = yes), Det == yes.
test(os_union3_det, (os_det(ord_union([a, c], [b, c, d], U)), U == [a, b, c, d]), true).
test(os_subtract_det, (os_det(ord_subtract([a, b, c], [b, d], D)), D == [a, c]), true).
test(os_inter3_det, (os_det(ord_intersection([a, b, c], [b, c, d], I)), I == [b, c]), true).
test(os_add_det, (os_det(ord_add_element([a, c], b, S)), S == [a, b, c]), true).
test(os_del_det, (os_det(ord_del_element([a, b, c], b, S)), S == [a, c]), true).
test(os_from_list_det, (os_det(list_to_ord_set([c, a, b, a], S)), S == [a, b, c]), true).
test(os_symdiff_det, (os_det(ord_symdiff([1, 2], [2, 3], X)), X == [1, 3]), true).
test(os_selectchk_det, (os_det(ord_selectchk(b, [a, b, c], R)), R == [a, c]), true).
test(os_selectchk_build_det, (os_det(ord_selectchk(b, S, [a, c])), S == [a, b, c]), true).
