% modules.pl の goal_use_list・goal_use_as・goal_use_not_exported。gp_hidden/0 は公開しない。
:- module(goal_pick, [gp_a/1, gp_b/1]).

gp_a(1).
gp_b(2).
gp_hidden.
