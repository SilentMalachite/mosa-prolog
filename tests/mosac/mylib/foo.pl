% $MOSA_LIB に置く利用者のライブラリ。中から素のパスで util を use_module する（計画 Task 10 直し 1）。
:- module(foo, [foo/1]).
:- use_module(util).

foo(X) :- util(X).
