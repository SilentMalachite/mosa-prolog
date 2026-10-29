% include_goal_dir.pl が include する。指示のゴールは読み込みの中で 1 回だけ、before/1 が見える所で動く。
:- ( retract(inc_count(N)) -> N1 is N + 1 ; N1 = 1 ), assertz(inc_count(N1)).
:- before(X), assertz(seen(X)).
