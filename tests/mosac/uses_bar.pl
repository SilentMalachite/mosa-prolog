% bar は中で素のパス 'sub/deep' を読み、プログラムは同じファイルを library('sub/deep') で読む（standalone_subdir）。
:- use_module(library(bar)).
:- use_module(library('sub/deep')).
:- initialization(main, main).

main :- bar(X), deep(Y), write(X-Y), nl.
