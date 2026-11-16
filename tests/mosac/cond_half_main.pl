:- dynamic(skip/0).
:- assertz(skip).
:- if(\+ skip).
:- use_module(cond_half_m).
:- endif.
:- use_module(cond_half_q).
:- initialization(main).
main :- findall(X-Y, (q(X), r(Y)), L), write(L), nl.
