:- module(mf_local, []).

:- multifile(mf_own:shared/1).

mf_own:shared(4).
mine(7).
