:- dynamic(skip/0).
:- assertz(skip).
:- if(skip).
:- else.
:- use_module(no_such_helper).
:- endif.
:- initialization(main).
main :- write(ran), nl.
