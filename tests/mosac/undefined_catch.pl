:- initialization(main, main).

t(X) :- catch((nosuch_goal, X), _, true).
r(X) :- catch(throw(e), _, (nosuch_recovery, X)).
main :- write(hello).
