% modules.pl の goal_use_conflict。gx_c/1 は modules.pl の局所の gx_c/1 と衝突する。
:- module(goal_conflict, [gx_c/1, gx_d/1]).

gx_c(module).
gx_d(1).
