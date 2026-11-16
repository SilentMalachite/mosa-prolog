:- dynamic(skip/0).
:- assertz(skip).
:- if(\+ skip).
:- use_module(cond_half_lib_m).
:- endif.
:- use_module(library(lists)).
:- initialization(main).
main :- append([a], [b], X), last(X, Y), write(X-Y), nl.
