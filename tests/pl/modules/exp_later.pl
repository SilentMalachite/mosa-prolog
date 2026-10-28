:- module(exp_later, [see_box/1]).
:- use_module(exp_box).

see_box(X) :- fresh_box(X).