:- initialization(main, main).
:- assertz(cond_flag).
:- if(cond_flag).
:- use_module(library(strings)).
:- endif.
main :- strings:upcase_atom(abc, X), write(X), nl.
