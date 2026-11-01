:- initialization(main, main).

late(X) :- catch(throw(e), _, (X, true)).
main :- late(halt(5)).
