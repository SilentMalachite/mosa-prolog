:- module(cond_goal_m, [hello/0]).
:- initialization((write(init_ran), nl)).
hello :- write(hello), nl.
