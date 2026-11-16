:- if((consult('cond_qf.pl'), \+ skip)).
:- use_module(no_such_helper).
:- endif.
:- initialization(main).
main :- write(ran), nl.
