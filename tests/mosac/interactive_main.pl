% mosac build --interactive の暫定の振る舞いの試験用。initialization(main, main) の main は動かさない（owner 未決）。
:- initialization(main, main).
main :- write('MARKER-MAIN'), nl.

app([], L, L).
app([H|T], L, [H|R]) :- app(T, L, R).
