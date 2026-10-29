% include したファイルの指示のゴールは、前の節がそろった所で 1 回だけ動く（フェーズ6 Task 5 の持ち越し b、Task 11 で直した）。
% 読み直しがあると、含めた側のゴールが 2 回動き、before/1 の無い所で動いて存在エラーになっていた。
:- dynamic(inc_count/1).
:- dynamic(seen/1).

before(1).

:- include('modules/include_goal').

test(include_goal_once, inc_count(N), solutions([N = 1])).
test(include_goal_in_context, seen(X), solutions([X = 1])).
