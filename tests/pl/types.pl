test(var_unbound, var(X), true).
test(var_atom, var(a), fail).
test(var_int, var(1), fail).
test(nonvar_atom, nonvar(a), true).
test(nonvar_var, nonvar(X), fail).
test(atom_nil, atom([]), true).
test(atom_a, atom(a), true).
test(atom_int, atom(1), fail).
test(integer_1, integer(1), true).
test(integer_float, integer(1.0), fail).
test(float_1, float(1.0), true).
test(float_int, float(1), fail).
test(atomic_nil, atomic([]), true).
test(atomic_list, atomic([a]), fail).
test(compound_nil, compound([]), fail).
test(compound_fun, compound(f(a)), true).
test(compound_list, compound([a]), true).
test(number_int, number(1), true).
test(number_float, number(1.0), true).
test(number_atom, number(a), fail).
test(callable_atom, callable(a), true).
test(callable_fun, callable(f(a)), true).
test(callable_number, callable(1), fail).
test(callable_var, callable(X), fail).
test(iso_callable_pair, callable((1, 2)), true).
test(list_proper, is_list([a]), true).
test(list_partial, is_list([a|b]), fail).
test(list_var, is_list(X), fail).
test(list_cycle, (L = [a|L], is_list(L)), fail).
% レビュー L3。is_list はリストの背骨だけを見る。要素が循環していても、背骨が [] で終わればリスト。前は項全体を
% acyclic_term で調べ、X = [X] も [f(f(...))] も偽にしたので、format/2 と msort/2 が型や具体化の誤りを出した。
test(l3_list_cyclic_element, (X = f(X), is_list([a, X, b])), true).
test(l3_list_self_element, (X = [X], is_list(X)), true).
test(l3_list_cyclic_spine_later, (L = [a, b|T], T = [c|T], is_list(L)), fail).
test(iso_nonvar_compound, nonvar(f(X)), true).
test(iso_atomic_float, atomic(1.5), true).
test(iso_compound_fun, compound(f(X)), true).
test(iso_callable_nil, callable([]), true).
test(iso_callable_list, callable([a]), true).
test(iso_number_neg, integer(-3), true).
test(iso_float_neg_zero, float(-0.0), true).
% 8.3.10.4 の Examples（Cor.2）。
test(iso_ground_int, ground(3), true).
test(iso_ground_partial, ground(a(1, _)), fail).
test(ground_atom, ground(a), true).
test(ground_var, ground(f(_)), fail).
test(ground_nested, ground(f(g(a), [b])), true).
test(ground_list_tail, ground([a|_]), fail).
test(ground_deep, (numlist_deep(30000, T), ground(T)), true).
test(ground_deep_var, (numlist_deep(30000, T0), T = f(T0, _), ground(T)), fail).
% ground_deep は既定のヒープで作れる深さ。辿りは明示のスタックで、深さで C++ のスタックを使わない。
numlist_deep(N, T) :- deep_acc(N, a, T).
deep_acc(0, T, T) :- !.
deep_acc(N, Acc, T) :- M is N - 1, deep_acc(M, f(Acc), T).
% レビュー M7: 巡回する項と、共有で指数に膨らむ項でも止まる。辿った複合項は二度辿らない。
% 辿りは最後の引数から降りるので、変数は最初の引数に置くと巡回や DAG を辿り終えた後に見つかる。
% DAG は述語の中で作って捨てる。差分テストは束縛を共有を展開して書くので、束縛に残すと終わらない。
m7_dag(0, L, L) :- !.
m7_dag(N, L, T) :- M is N - 1, m7_dag(M, L, T0), T = f(T0, T0).
m7_dag_ground(N, Leaf) :- m7_dag(N, Leaf, T), ground(T).
m7_dag_late_var(N) :- m7_dag(N, a, T), ground(g(_, T)).
test(m7_ground_cyclic, (X = f(X), ground(X)), true).
test(m7_ground_cyclic_shared, (X = f(X, X), ground(X)), true).
test(m7_ground_cyclic_var, (X = f(_, X), ground(X)), fail).
test(m7_ground_cyclic_list, (X = [a|X], ground(X)), true).
test(m7_ground_lasso_var, (X = [a|X], ground(f(_, [b|X]))), fail).
test(m7_ground_mutual, (X = g(Y, a), Y = h(X), ground(X)), true).
test(m7_ground_dag, m7_dag_ground(64, a), true).
test(m7_ground_dag_late_var, m7_dag_late_var(64), fail).

% 8.3.11.4 acyclic_term/1（Cor.2）。リポジトリに規格本文が無いので、iso_ の 2 行は同梱の Logtalk 準拠スイート
% tests/conformance/logtalk/prolog/predicates/acyclic_term_1/tests.lgt の iso_acyclic_term_1_01 と _02 から写す。
% 02 の X = f(X) は規格では「STO; Undefined」。Mosa は循環項を作れるので、acyclic_term/1 は失敗する。
test(iso_acyclic_term_1_01, acyclic_term(a(1, _)), true).
test(iso_acyclic_term_1_02, (X = f(X), acyclic_term(X)), fail).
% 以下は Mosa 自前のテスト。
test(acyclic_atom, acyclic_term(a), true).
test(acyclic_var, acyclic_term(_), true).
test(acyclic_compound, acyclic_term(f(X, _, g(X))), true).
test(acyclic_list_cyclic, (X = [a|X], acyclic_term(X)), fail).
test(acyclic_dag, (X = f(Y, Y), acyclic_term(X)), true).
test(acyclic_cycle_inside, (Y = g(Y), acyclic_term(f(a, Y))), fail).
