:- module(flag_inc, []).
:- set_prolog_flag(double_quotes, chars).
:- include('flag_inc_in.pl').
:- initialization((current_prolog_flag(double_quotes, chars) -> true ; throw(leaked))).
