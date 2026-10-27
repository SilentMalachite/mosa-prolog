:- module(expose, [show/2, run/1, call_one/1, take/2, pi/2, left_goal/1, right_goal/1,
                 colon_run/1, expose_items/1]).
:- meta_predicate show(0, ?).
:- meta_predicate run(0).
:- meta_predicate call_one(1).
:- meta_predicate take(^, ?).
:- meta_predicate pi(//, ?).
:- meta_predicate left_goal(0), right_goal(0), colon_run(:).

show(Goal, Goal).
run(Goal) :- Goal.
call_one(Pred) :- call(Pred, 1).
take(Goal, Goal).
pi(Term, Term).
left_goal(Goal) :- Goal.
right_goal(Goal) :- Goal.
colon_run(Goal) :- Goal.
expose_item(a).
expose_items(Xs) :- findall(X, expose_item(X), Xs).
