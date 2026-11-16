:- initialization(main, main).
:- assertz(cond_op).
:- if(cond_op).
:- op(700, xfx, ===>).
:- endif.
r(a ===> b).
main :- r(X), write(X), nl.
