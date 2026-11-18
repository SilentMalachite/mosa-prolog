:- initialization(main, main).
p(1).
main :- spy(p/1), trace, p(X), write(X), nl, notrace, nodebug,
    current_prolog_flag(debug, F), write(F), nl, debugging.
