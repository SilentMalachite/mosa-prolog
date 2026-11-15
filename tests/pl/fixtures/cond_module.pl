:- module(cond_module, [cond_where/1]).
cond_here.
:- if(cond_here).
cond_where(module).
:- else.
cond_where(user).
:- endif.
