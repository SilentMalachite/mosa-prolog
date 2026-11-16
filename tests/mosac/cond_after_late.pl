:- dynamic(skip/0).
:- assertz(skip).
:- if(\+ skip).
:- endif.
q(1).
p.
q(2).
:- initialization(main).
main :- write(ran), nl.
