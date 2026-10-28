% フェーズ6 Task 4。mosac が頭部と連言を C++ に訳す述語の素材。今はインタプリタで流す。
% Task 11 から mosatest --diff でも流し、生成コードとインタプリタの答えを比べる。
% 変数どうしの標準順序は番地で決まるので、節の変数を作る順と束ねる向きも答えに出る（計画の Review Focus 1）。
:- use_module(library(lists)).

% 事実
color(red).
color(green).
color(blue).

% 規則（頭部の変数どうしの単一化を含む）
app([], L, L).
app([H|T], L, [H|R]) :- app(T, L, R).

% 深さのある構造
shape(box(size(2, 3), at(point(0, 0), [corner(a), corner(b)]))).

% 浮動小数
ratio(half, 0.5).
ratio(quarter, 0.25).

% 負の整数。2 つ目は即値の範囲（61 ビット）の外で、箱の整数になる
offset(left, -3).
offset(far, -1152921504606846977).

% 日本語の原子の頭部
親(太郎, 花子).
親(花子, 次郎).
祖父母(X, Z) :- 親(X, Y), 親(Y, Z).

% 節の変数はスロットの順に作る。スロットは本体から振るので L、B、A の順で、B が A より古い
mk(L) :- L = [B, A], g(A), g(B).
% 頭部だけに出る変数もスロットは本体の後。A が 0、B が 1 なので A が古い
mk_head([B, A]) :- g(A), g(B).
g(_).

test(native_fact, color(green), true).
test(native_fact_all, color(C), solutions([C = red, C = green, C = blue])).
test(native_fact_none, color(black), fail).
test(native_rule, app([1, 2], [3], L), solutions([L = [1, 2, 3]])).
test(native_rule_split, app(X, Y, [1, 2]),
     solutions([(X = [], Y = [1, 2]), (X = [1], Y = [2]), (X = [1, 2], Y = [])])).
test(native_deep, shape(box(size(W, H), at(point(0, Y), [_, corner(C)]))),
     solutions([(W = 2, H = 3, Y = 0, C = b)])).
test(native_deep_build, (shape(S), S == box(size(2, 3), at(point(0, 0), [corner(a), corner(b)]))), true).
test(native_deep_mismatch, shape(box(size(2, 4), _)), fail).
test(native_float, ratio(half, R), solutions([R = 0.5])).
test(native_float_match, ratio(N, 0.25), solutions([N = quarter])).
test(native_float_int, ratio(_, 1), fail).
test(native_negative, offset(left, V), solutions([V = -3])).
test(native_negative_box, offset(D, -1152921504606846977), solutions([D = far])).
test(native_japanese, 親(太郎, Who), solutions([Who = 花子])).
test(native_japanese_rule, 祖父母(G, 次郎), solutions([G = 太郎])).
test(native_var_order, (mk(L), msort(L, S), S == L), true).
test(native_var_order_head, (mk_head(L), msort(L, S), S == L), fail).
% 頭部の 2 つ目の H は節の側（第1引数から得た X）が左なので、X が Y を指す。
% 代表は Y で、間にある W より新しい。向きが逆なら代表は X になり W より古い
test(native_var_head, (T = t(X, W, Y), app([X], [], [Y]), X @> W, T = t(_, _, _)), true).
