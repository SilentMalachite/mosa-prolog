len([], 0).
len([_|T], N) :- len(T, M), N is M + 1.
c(1).
c(2).
q :- p.
p :- throw(oops).
double(X, Y) :- Y is X * 2.
deep(f(f(f(f(f(f(f(f(f(f(f(a)))))))))))).
:- use_module(library(apply)).
:- use_module(library(lists)).
