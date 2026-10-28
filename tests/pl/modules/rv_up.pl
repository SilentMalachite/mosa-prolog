:- module(rv_up, [rv_ask/1]).
% rv_helper/1 は user にだけある。既定の取り込み元として見える。
rv_ask(X) :- rv_helper(X).
