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
% フェーズ6 Task 7。制御構文を生成コードで動かす。構文ごとに 1 件。
% カット。節の境界まで切るので、2 つ目の節の ! は 3 つ目の節の候補を捨てる
cutc(first).
cutc(X) :- !, X = second.
cutc(third).
% 呼び出しのあとのカット。再開の状態では境界が継続枠から来る
first_color(C) :- color(C), !.

test(native_cut, cutc(X), solutions([X = first, X = second])).
test(native_cut_third, cutc(third), fail).
test(native_cut_after_call, first_color(C), solutions([C = red])).

% 選言。両方の枝が節の変数を使い、枝のあとのゴールへ続く
either(X, Y) :- (X = a ; X = b), Y = X.
% 枝の中のカットは節の境界まで切る（透明）。else の枝も 2 つ目の節も捨てる
or_cut(X) :- (X = 1, ! ; X = 2).
or_cut(3).
% 呼び出しで終わらない枝（true）は、構文のあとのゴールへ直に続く
or_true(X) :- (X = 1 ; true), X = 2.

test(native_or, either(X, Y), solutions([(X = a, Y = a), (X = b, Y = b)])).
test(native_or_cut, or_cut(X), solutions([X = 1])).
test(native_or_true, or_true(X), solutions([X = 2])).

% if-then-else。入れ子の else も 1 つの状態の列になる
sign(X, S) :- (X > 0 -> S = positive ; X < 0 -> S = negative ; S = zero).
% 条件は最初の解で確定し、条件の選択点は捨てる
pick_first(X) :- (color(X) -> true ; X = none).
% 条件の中のカットは条件に閉じる。外の選択点（X の選言）は残る
ite_local(X, Y) :- (X = 1 ; X = 2), ((Y = a, ! ; Y = b) -> true ; true).
% then の中のカットは節の境界まで切る。2 つ目の節を捨てる
then_cut(X) :- (true -> X = 1, ! ; true).
then_cut(2).
% else の無い -> は条件が失敗すれば失敗する
only_then(X, Y) :- (X = 1 -> Y = one).

test(native_ite, sign(5, S), solutions([S = positive])).
test(native_ite_nested, sign(-5, S), solutions([S = negative])).
test(native_ite_else, sign(0, S), solutions([S = zero])).
test(native_ite_commit, findall(X, pick_first(X), L), solutions([L = [red]])).
test(native_ite_cond_cut, findall(X-Y, ite_local(X, Y), L), solutions([L = [1-a, 2-a]])).
test(native_ite_then_cut, findall(X, then_cut(X), L), solutions([L = [1]])).
test(native_if_then, only_then(1, Y), solutions([Y = one])).
test(native_if_then_fail, only_then(2, _), fail).

% ソフトカット。条件の解をすべて残し、else は条件が一度も成功しなかったときだけ動く
soft_all(X) :- (color(X) *-> true ; X = none).
soft_none(X) :- (fail *-> X = yes ; X = none).
% else の無い *->
soft_only(X) :- (color(X) *-> X \== green).

test(native_soft, findall(X, soft_all(X), L), solutions([L = [red, green, blue]])).
test(native_soft_else, soft_none(X), solutions([X = none])).
test(native_soft_bare, findall(X, soft_only(X), L), solutions([L = [red, blue]])).

% 否定。中のゴールが成功すれば失敗し、失敗すれば束縛を残さず続く
not_color(X) :- \+ color(X).
not_bound(Y) :- \+ (X = 1, X = 2), Y = ok.
% 中のカットは否定に閉じる。外の選択点（X の選言）は残る
not_cut(X) :- (X = 1 ; X = 2), \+ (color(_), !, fail).
% 二重否定は束縛を残さない
not_twice(X) :- \+ \+ color(X).
% 中が変数なら呼び出しのときに決まる。組み込みの \+/1 に任せる
not_var(G) :- \+ G.

test(native_not, not_color(black), true).
test(native_not_fail, not_color(red), fail).
test(native_not_unbind, not_bound(Y), solutions([Y = ok])).
test(native_not_cut, findall(X, not_cut(X), L), solutions([L = [1, 2]])).
test(native_not_twice, (not_twice(X), var(X)), true).
test(native_not_var, not_var(color(black)), true).
test(native_not_var_fail, not_var(color(red)), fail).
test(native_not_var_inst, not_var(_), error(instantiation_error)).

% 変数のゴール。call/1 と同じで、中のカットは呼び出しに閉じ、外の選択点は残る（7.8.3）
apply_goal(G) :- G.
apply_cut(X, G) :- (X = 1 ; X = 2), G.
apply_then(G, Y) :- G, Y = done.
% call/N は余分の引数を足して呼ぶ組み込み
call_extra(X) :- call(color, X).
% リストのゴールは '.'/2。定義が無いので存在エラー
load_list(F) :- [F].

test(native_var_goal, apply_goal(color(C)), solutions([C = red, C = green, C = blue])).
test(native_var_goal_cut, findall(X, apply_cut(X, !), L), solutions([L = [1, 2]])).
test(native_var_goal_then, apply_then(true, Y), solutions([Y = done])).
test(native_var_goal_inst, apply_goal(_), error(instantiation_error)).
test(native_var_goal_type, apply_goal(3), error(type_error(callable, 3))).
test(native_call_extra, call_extra(X), solutions([X = red, X = green, X = blue])).
test(native_list_goal, load_list(x), error(existence_error(procedure, '.'/2))).

test(native_var_order, (mk(L), msort(L, S), S == L), true).
test(native_var_order_head, (mk_head(L), msort(L, S), S == L), fail).
% 頭部の 2 つ目の H は節の側（第1引数から得た X）が左なので、X が Y を指す。
% 代表は Y で、間にある W より新しい。向きが逆なら代表は X になり W より古い
test(native_var_head, (T = t(X, W, Y), app([X], [], [Y]), X @> W, T = t(_, _, _)), true).
