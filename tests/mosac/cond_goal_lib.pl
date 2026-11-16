:- initialization(main, main).
:- assertz(cond_goal_flag).
:- if((cond_goal_flag, use_module(library(strings)))).
:- endif.
main :- strings:upcase_atom(abc, X), write(X), nl.
