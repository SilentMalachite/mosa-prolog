% $MOSA_LIB のライブラリ foo を使う。foo は中で素のパスの use_module(util) をする（standalone_plain_use）。
:- use_module(library(foo)).
:- initialization(main, main).

main :- foo(X), write(X), nl.
