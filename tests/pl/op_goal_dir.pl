% 一般の指示のゴールが定義した演算子を、後の項の読みに使う（フェーズ6 Task 5 の持ち越し a、Task 11 で直した）。
% mosatest はファイルを 1 回だけ読み、指示をその場で動かすので、断片の読み直しで op を飛ばすことはない。
:- true, op(700, xfx, ===>).

arrow(a ===> b).

test(op_from_goal_directive, arrow(X), solutions([X = (a ===> b)])).
test(op_from_goal_current, current_op(700, xfx, ===>), true).
