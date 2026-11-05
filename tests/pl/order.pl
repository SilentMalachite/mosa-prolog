test(float_before_int, (1.0 @< 1), true).
test(signed_zero, (0.0 @< -0.0), fail).
test(signed_zero_eq, (0.0 == -0.0), fail).
test(term_eq_number, (1 == 1.0), fail).
test(atom_order, (a @< b), true).
test(compare_same, compare(=, a, a), true).
test(compare_less, compare(<, a, b), true).
test(not_term_eq, (a \== b), true).
test(term_eq, (a == a), true).
test(ge, (b @>= a), true).
test(iso_int_after_float, (1 @< 1.0), fail).
test(iso_atom_le, (a @=< a), true).
test(iso_compare_gt, compare(>, b, a), true).
test(iso_fun_order, (f(a) @< f(b)), true).
test(iso_list_eq, ([a] == [a]), true).
test(iso_list_lt, ([a] @< [b]), true).
% レビュー L2（iso-notes 7.2.2 の決定 A）。-0.0 と 0.0 は別の項で、標準順序では -0.0 が先。算術の比べでは等しい。
test(l2_zero_order, (-0.0 @< 0.0), true).
test(l2_zero_compare, compare(O, 0.0, -0.0), solutions([O = (>)])).
test(l2_zero_unify, (-0.0 = 0.0), fail).
test(l2_zero_unify_same, (X is -(0.0), X = -0.0), true).
test(l2_zero_identical_same, (-0.0 == -0.0), true).
test(l2_zero_arith, (-0.0 =:= 0.0), true).
test(l2_zero_sort, (sort([0.0, -0.0, 0.0], L), L == [-0.0, 0.0]), true).
test(l2_zero_keysort, (keysort([0.0-a, -0.0-b, 0.0-c], L), L == [-0.0-b, 0.0-a, 0.0-c]), true).
test(l2_zero_setof, (setof(X, (X = 0.0 ; X = -0.0 ; X = 0.0), L), L == [-0.0, 0.0]), true).
test(l2_zero_before_int, (-0.0 @< 0, 0.0 @< 0), true).
test(l2_zero_struct, (f(-0.0) \== f(0.0), f(-0.0) @< f(0.0)), true).
