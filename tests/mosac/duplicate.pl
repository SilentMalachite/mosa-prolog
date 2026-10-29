% 埋め込みの鍵の重なり（mosac_emit_duplicate）。include した library/x.pl と、MOSA_LIB=mylib の library(x) が
% どちらも library/x.pl で埋め込まれるので、翻訳のときに断る。
:- include('library/x.pl').
:- use_module(library(x)).
:- initialization(main, main).

main :- x_local, x.
