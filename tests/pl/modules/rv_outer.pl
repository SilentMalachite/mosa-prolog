:- module(rv_outer, [rv_outer_p/1]).
:- use_module(rv_inner).
rv_outer_p(X) :- rv_inner_p(X).
