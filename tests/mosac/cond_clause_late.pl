:- dynamic(skip/0).
:- assertz(skip).
:- retract(skip).
:- include(cond_clause_late_part).
:- initialization(main).
main :- write(ran), nl.
