% mosac build --interactive の試験用。app/3 は翻訳され、extra/1 は動的なので assertz で書き換えられる。
app([], L, L).
app([H|T], L, [H|R]) :- app(T, L, R).

:- dynamic(extra/1).
extra(1).
