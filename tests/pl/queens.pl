range(From, To, []) :- From > To, !.
range(From, To, [From|Rest]) :-
    Next is From + 1,
    range(Next, To, Rest).

take(X, [X|Xs], Xs).
take(X, [Y|Ys], [Y|Zs]) :- take(X, Ys, Zs).

perm([], []).
perm(Xs, [Y|Ys]) :- take(Y, Xs, Zs), perm(Zs, Ys).

safe([]).
safe([Q|Qs]) :- safe(Qs), \+ attack(Q, Qs).

attack(Q, Qs) :- attack(Q, 1, Qs).
attack(Q, N, [Q1|_]) :- abs(Q - Q1) =:= N.
attack(Q, N, [_|Qs]) :- N1 is N + 1, attack(Q, N1, Qs).

queens(N, Qs) :- range(1, N, Ns), perm(Ns, Qs), safe(Qs).

test(queens4, queens(4, Qs), solutions([Qs = [2, 4, 1, 3]])).
test(queens8, queens(8, Qs), solutions([Qs = [1, 5, 8, 6, 3, 7, 2, 4]])).
