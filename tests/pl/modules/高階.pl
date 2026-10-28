% maplist/2 の仮。Task 13 で library(apply) の maplist/2 に替える。家系.pl が取り込む。
:- module(高階, [仮写像/2]).
:- meta_predicate 仮写像(1, ?).

仮写像(_, []).
仮写像(G, [X|Xs]) :- call(G, X), 仮写像(G, Xs).
