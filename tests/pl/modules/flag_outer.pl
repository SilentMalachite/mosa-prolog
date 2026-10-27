:- module(flag_outer, []).
:- set_prolog_flag(double_quotes, chars).
:- ['flag_inner.pl'].
seen("cd").
