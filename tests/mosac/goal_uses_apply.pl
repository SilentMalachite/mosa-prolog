:- initialization(main, main).
main :- use_module(library(apply)), apply:maplist(atom_length, [ab, c], L), write(L), nl.
