:- use_module(library(lists)).
:- initialization(main, main).
main :- append(X, [c], [a, b, c]), write(X), nl.
