:- initialization(main, main).
:- use_module(cond_export_mod).
main :- answer(X), write(X), nl.
