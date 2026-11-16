:- initialization(main, main).
:- dynamic(cond_goal_skip/0).
:- assertz(cond_goal_skip).
:- if((\+ cond_goal_skip, use_module(cond_goal_m))).
:- endif.
main :- catch(cond_goal_m:hello, error(E, _), (write(E), nl)).
