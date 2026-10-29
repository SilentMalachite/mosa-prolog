:- initialization(main, main).
:- true, op(700, xfx, ===>).
r(a ===> b).
main :- r(X), X = (A ===> B), write(A-B), nl.
