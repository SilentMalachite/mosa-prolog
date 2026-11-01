:- initialization(main, main).

late(X) :- catch((X, true), _, true).
main :- late(halt(4)).
