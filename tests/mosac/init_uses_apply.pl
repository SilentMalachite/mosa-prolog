:- initialization(use_module(library(apply))).
:- initialization(main, main).
main :- apply:maplist(atom_length, [abc], L), write(L), nl.
