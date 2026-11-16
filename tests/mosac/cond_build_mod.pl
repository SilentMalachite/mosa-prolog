:- initialization(main, main).
:- dynamic(cond_skip/0).
:- assertz(cond_skip).
:- if(\+ cond_skip).
:- use_module(cond_init_mod).
:- endif.
main :- catch(cond_init_mod:hello, error(E, _), (write(E), nl)), catch(hello, error(F, _), (write(F), nl)).
