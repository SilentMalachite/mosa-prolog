:- module(uselib, [pick/2]).
pick(X, Y) :- ( X > 0 -> Y = pos ; Y = neg ).
