% modules.pl の goal_use_flags。ゴールで読んでも、このファイルの double_quotes は呼んだ側へ残らない。
:- module(goal_flags, [gf/1]).
:- set_prolog_flag(double_quotes, atom).

gf("x").
