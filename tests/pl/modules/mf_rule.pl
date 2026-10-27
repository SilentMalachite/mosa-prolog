:- module(mf_rule, []).

:- multifile(mf_rule_own:ruled/1).

mf_rule_own:ruled(X) :- X = 1.
