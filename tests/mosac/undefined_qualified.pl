:- initialization(main, main).

c(X) :- user:(nosuch_conj, X).
i(X) :- user:(true -> nosuch_then ; X).
n(X) :- user:(X, \+ nosuch_not).
q :- user:nosuch_plain.
main :- write(hello).
