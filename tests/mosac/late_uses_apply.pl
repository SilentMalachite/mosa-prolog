:- initialization(main, main).
main :-
    catch(apply:maplist(atom_length, [a], _), error(E1, _), true), write(E1), nl,
    catch(lists:append([a], [b], _), error(E2, _), true), write(E2), nl.
never :- use_module(library(apply)).
