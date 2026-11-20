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
% レビュー H-04（ADR 0029）。巡回する項は、展開した木を深さ優先に並べた節点で比べ、それがいつまでも等しく続くときだけ、
% 段ごとに並べた節点（いちばん浅い違い、次にいちばん左の違い）で比べる。前は、もう一度会った組を等しいとみなしたので、
% 次の 3 つ組は A < B、B < C、C < A になり、sort/2 の答えが入力の順に依った。今は A < C < B。
h04_triple(A, B, C) :- A = f(A, b), B1 = f(B1, b), B = f(B1, B), C = f(C1, C), C1 = f(C, a).
h04_sorted(L, S) :- sort(L, S).
test(h04_triple_order, (h04_triple(A, B, C), compare(O1, A, B), compare(O2, B, C), compare(O3, A, C)),
     solutions([(O1 = (<), O2 = (>), O3 = (<))])).
test(h04_triple_sort, (h04_triple(A, B, C), Want = [A, C, B],
                       h04_sorted([A, B, C], S1), S1 == Want, h04_sorted([A, C, B], S2), S2 == Want,
                       h04_sorted([B, A, C], S3), S3 == Want, h04_sorted([B, C, A], S4), S4 == Want,
                       h04_sorted([C, A, B], S5), S5 == Want, h04_sorted([C, B, A], S6), S6 == Want), true).
test(h04_triple_sort4, (h04_triple(A, B, C), sort(0, @=<, [C, B, A, C], S), S == [A, C, C, B]), true).
test(h04_triple_keysort, (h04_triple(A, B, C), keysort([C-1, B-2, A-3], S), S == [A-3, C-1, B-2]), true).
% 答えが変わる組。A と A1 は第 1 引数が互いを指すので、引数ごとの規則ではどちらの答えも決まらない。
test(h04_mutual, (A = f(A1, b), A1 = f(A, c), compare(O1, A, A1), compare(O2, A1, A)),
     solutions([(O1 = (<), O2 = (>))])).
test(h04_shallow_first, (A = f(A, b), B = f(B, c), compare(O, f(A, z), f(B, a))), solutions([O = (>)])).
% 深さ優先の歩みが違いに届く組は前と同じ。
test(h04_walk_decides, (A = f(A, b), compare(O, A, f(f(z, b), b))), solutions([O = (>)])).
test(h04_unrolled_equal, (X = f(X), Y = f(f(Y)), compare(O, X, Y), X == Y), solutions([O = (=)])).
% 答えは項の共有に依らない。同じセルの巡回する部分項 X も、等しい別の項 Y も、深さ優先の並びをそこで止める。
% 比べ終えた組 (P, Q) が中に X を持つときも同じ。巡回しない共有の部分項は、深さ優先の歩みがそのまま飛ばす。
test(h04_shared_cyclic, (X = f(X), Y = f(Y), compare(O1, f(X, k(b), a), f(X, k(a), b)),
                         compare(O2, f(Y, k(b), a), f(X, k(a), b))), solutions([(O1 = (<), O2 = (<))])).
test(h04_shared_inside, (X = f(X), P = p(X), Q = p(X), X1 = f(X1), X2 = f(X2),
                         compare(O1, f(P, P, k(b), a), f(Q, Q, k(a), b)),
                         compare(O2, f(p(X1), p(X1), k(b), a), f(p(X2), p(X2), k(a), b))),
     solutions([(O1 = (<), O2 = (<))])).
test(h04_shared_acyclic, (Z = g(c), compare(O, f(Z, k(b), a), f(Z, k(a), b))), solutions([O = (>)])).
% 有限の項は 7.2 のまま。比べ終えて等しかった共有の部分項にまた会っても、巡回とはみなさない。深さ 64 の上と下で見る。
h04_wrap(0, X, X) :- !.
h04_wrap(N, X, h(Y)) :- N1 is N - 1, h04_wrap(N1, X, Y).
h04_shared(N, O) :-
    h04_wrap(N, f(g(b), a), Z1), h04_wrap(N, f(g(b), a), Z2),
    h04_wrap(N, p(Z1, Z1, k(b), a), L), h04_wrap(N, p(Z2, Z2, k(a), b), R), compare(O, L, R).
test(h04_shared_shallow, h04_shared(10, O), solutions([O = (>)])).
test(h04_shared_deep, h04_shared(100, O), solutions([O = (>)])).
% 決まった擬似乱数で、節点 4 つのグラフを 12 個作る。節点は f/2、g/1、a、b のどれかで、子は同じグラフの節点。同じ形を
% 2 組の節点に交互に置いた写しは周期が 2 倍で、展開した木は元と同じになる。集まりを並べた後のどの 2 つも < で、> は
% 逆で、= は単一化が成り立つ組だけ。写しは元と同じ答えになる。
h04_next(S0, S) :- S is (S0 * 1103515245 + 12345) mod 2147483648.
h04_pick(S0, S, I) :- h04_next(S0, S), I is (S >> 16) mod 4 + 1.
h04_shapes(0, S, S, []) :- !.
h04_shapes(N, S0, S, [Shape|Shapes]) :-
    h04_next(S0, S1), Kind is (S1 >> 16) mod 8, h04_pick(S1, S2, I), h04_pick(S2, S3, J),
    h04_shape(Kind, I, J, Shape), N1 is N - 1, h04_shapes(N1, S3, S, Shapes).
h04_shape(Kind, I, J, f(I, J)) :- Kind < 4, !.
h04_shape(Kind, I, _, g(I)) :- Kind < 6, !.
h04_shape(6, _, _, a) :- !.
h04_shape(7, _, _, b).
h04_nth(1, [X|_], X) :- !.
h04_nth(N, [_|T], X) :- N1 is N - 1, h04_nth(N1, T, X).
h04_bind([], [], _).
h04_bind([Shape|Shapes], [Node|Nodes], Other) :- h04_node(Shape, Other, Node), h04_bind(Shapes, Nodes, Other).
h04_node(f(I, J), Other, f(X, Y)) :- h04_nth(I, Other, X), h04_nth(J, Other, Y).
h04_node(g(I), Other, g(X)) :- h04_nth(I, Other, X).
h04_node(a, _, a).
h04_node(b, _, b).
h04_graphs(0, _, [], []) :- !.
h04_graphs(N, S0, Terms, Copies) :-
    h04_shapes(4, S0, S1, Shapes), Ns = [_, _, _, _], D0 = [_, _, _, _], D1 = [_, _, _, _],
    h04_bind(Shapes, Ns, Ns), h04_bind(Shapes, D0, D1), h04_bind(Shapes, D1, D0),
    N1 is N - 1, h04_graphs(N1, S1, Terms0, Copies0),
    h04_append(Ns, Terms0, Terms), h04_append(D0, Copies0, Copies).
h04_append([], L, L).
h04_append([X|T], L, [X|R]) :- h04_append(T, L, R).
h04_ascending([]).
h04_ascending([X|T]) :- h04_after(T, X), h04_ascending(T).
h04_after([], _).
h04_after([Y|T], X) :- compare(<, X, Y), compare(>, Y, X), \+ X = Y, h04_after(T, X).
% 項 T と、並べた集まり S の各要素との答えが、写し C でも同じで、= は 1 つだけで単一化も成り立つ。
h04_placed([], [], _).
h04_placed([T|Ts], [C|Cs], S) :- h04_same(S, T, C, 0, 1), h04_placed(Ts, Cs, S).
h04_same([], _, _, Equal, Equal).
h04_same([E|Es], T, C, Equal0, Equal) :-
    compare(O, T, E), compare(O, C, E),
    ( O == (=) -> \+ \+ T = E, Equal1 is Equal0 + 1 ; \+ T = E, Equal1 = Equal0 ),
    h04_same(Es, T, C, Equal1, Equal).
% T を共有した 2 つの項と、片方の T を写しに替えた 2 つの項は、同じ答えになる。
h04_shared_all([], []).
h04_shared_all([T|Ts], [C|Cs]) :-
    compare(O, w(T, k(b), a), w(T, k(a), b)), compare(O, w(C, k(b), a), w(T, k(a), b)),
    h04_shared_all(Ts, Cs).
test(h04_generated, (h04_graphs(12, 2026, Terms, Copies), h04_sorted(Terms, S), h04_ascending(S),
                     h04_placed(Terms, Copies, S), h04_shared_all(Terms, Copies)), true).
