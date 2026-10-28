:- module(rv_init, []).
:- dynamic(rv_ran/0).
:- initialization(rv_go).
rv_go :- assertz(rv_ran).
