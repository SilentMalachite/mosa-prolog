:- dynamic(skip/0).
:- assertz(skip).
q(1).
:- if(\+ skip).
p(1).
:- endif.
q(2).
:- initialization(main).
main :- findall(X, q(X), L), write(L), nl.
