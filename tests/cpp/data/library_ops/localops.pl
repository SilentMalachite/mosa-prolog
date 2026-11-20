:- module(localops, [t/1]).
:- op(700, xfx, ~~).
t(a ~~ b).
