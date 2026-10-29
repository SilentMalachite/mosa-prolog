:- include('a.pl').
:- ensure_loaded(b).
:- initialization(main, main).
main :- a(X), b(Y), write(X-Y), nl.
