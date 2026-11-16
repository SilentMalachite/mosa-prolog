:- module(cond_export_mod, [answer/1]).
:- assertz(cond_mod_ready).
:- if(cond_mod_ready).
answer(42).
:- endif.
