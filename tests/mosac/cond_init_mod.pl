:- module(cond_init_mod, [hello/0]).
:- initialization((write(init_ran), nl)).
hello :- write(hello), nl.
