nrev([], []).
nrev([H|T], R) :- nrev(T, S), append(S, [H], R).

append([], L, L).
append([H|T], L, [H|R]) :- append(T, L, R).

test(nrev3, nrev([1, 2, 3], R), solutions([R = [3, 2, 1]])).
