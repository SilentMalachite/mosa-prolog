:- dynamic(skip/0).
:- assertz(skip).
:- include(cond_clause_fail_part).
:- initialization(main).
main :- write(ran), nl.
