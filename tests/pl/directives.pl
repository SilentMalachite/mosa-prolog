% 7.4.2 discontiguous/1。指示だけ。同じテキストで、隙間より前に宣言した述語は節を離してよい。
% 宣言が無い述語は、診断「述語 名前/アリティ の節が離れている」のまま失敗する。
% 宣言は手続きを定義しない。次の consult は、multifile が無いので置き換える。

:- discontiguous(dc_p/1).

dc_p(1).
dc_q(1).
dc_p(2).

:- discontiguous([dc_a/1, dc_b/0]).

dc_a(1).
dc_mid(1).
dc_b.
dc_a(2).
dc_late_gap(1).
dc_b.

:- discontiguous(dc_rule/1).

dc_rule(1).
dc_side(9).
dc_rule(X) :- dc_side(X).

:- discontiguous(dc_only/1).

dc_after(1).

:- discontiguous(dc_after/1).

dc_between(1).
dc_after(2).

test(dc_order, (findall(X, dc_p(X), L), L == [1, 2]), true).
test(dc_neighbor, dc_q(1), true).
test(dc_list_a, (findall(X, dc_a(X), L), L == [1, 2]), true).
test(dc_list_b, (findall(t, dc_b, L), L == [t, t]), true).
test(dc_rule, (findall(X, dc_rule(X), L), L == [1, 9]), true).
test(dc_only_absent, current_predicate(dc_only/1), fail).
test(dc_only_call, dc_only(_), error(existence_error(procedure, dc_only/1))).
test(dc_after_first, (findall(X, dc_after(X), L), L == [1, 2]), true).
