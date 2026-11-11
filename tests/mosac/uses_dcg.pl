:- use_module(library(dcg)).
:- initialization(main, main).
main :- expand_term((a --> [x]), C), C = (H :- _), functor(H, N, A), write(N/A), nl.
