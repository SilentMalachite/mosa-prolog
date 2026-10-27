use_apply :- use_module(library(apply)).

ok_item(a).
ok_item(b).

% maplist/2 は呼び出し元の述語を各要素へ適用する。本体は apply で動く。
test(apply_maplist2, (use_apply, maplist(ok_item, [a, b])), true).
test(apply_maplist2_miss, (use_apply, maplist(ok_item, [a, c])), fail).
test(apply_maplist2_empty, (use_apply, maplist(ok_item, [])), true).

add1(X, Y) :- Y is X + 1.

parent(taro, jiro).
parent(taro, hanako).

% maplist/3 は二つのリストの対応する要素へ述語を適用する。
test(apply_maplist3, (use_apply, maplist(add1, [1, 2], [2, 3])), true).

sum3(A, B, C) :- C is A + B.

% maplist/4 は三つのリストの対応する要素へ述語を適用する。
test(apply_maplist4, (use_apply, maplist(sum3, [1, 2], [10, 20], [11, 22])), true).

sum4(A, B, C, D) :- D is A + B + C.

% maplist/5 は四つのリストの対応する要素へ述語を適用する。
test(apply_maplist5, (use_apply, maplist(sum4, [1, 2], [10, 20], [100, 200], [111, 222])), true).

% 部分適用は呼び出し元の節。parent/2 は user にあり、maplist の本体は apply で動く。
test(apply_maplist_partial, (use_apply, maplist(parent(taro), [jiro, hanako])), true).
test(apply_maplist3_out, (use_apply, maplist(add1, [1, 2], Ys), Ys = [2, 3]), true).
test(apply_maplist3_short, (use_apply, maplist(add1, [1, 2], [2])), fail).

push(X, Acc, [X|Acc]).

% foldl/4 は左から累積する。要素、前の値、次の値の順で呼び出し元を呼ぶ。
test(apply_foldl4, (use_apply, foldl(push, [a, b, c], [], [c, b, a])), true).

push2(X, Y, Acc, [[X, Y]|Acc]).

% foldl/5 は二つのリストを左から組にして畳む。
test(apply_foldl5, (use_apply, foldl(push2, [a, b], [1, 2], [], [[b, 2], [a, 1]])), true).

push3(X, Y, Z, Acc, [[X, Y, Z]|Acc]).

% foldl/6 は三つのリストを左から組にして畳む。
test(apply_foldl6, (use_apply, foldl(push3, [a, b], [1, 2], [x, y], [], [[b, 2, y], [a, 1, x]])), true).
test(apply_foldl4_empty, (use_apply, foldl(push, [], 0, 0)), true).
test(apply_foldl5_short, (use_apply, foldl(push2, [a, b], [1], [], _)), fail).
