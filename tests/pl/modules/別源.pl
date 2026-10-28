% as で別名を付けて取り込んだ述語への assert、retract、clause を見るための取り込み元。modules.pl が読む。
:- module(別源, [動/1, 静/1, 型/2]).
:- dynamic(動/1).
:- meta_predicate 型(0, ?).

動(1).
動(2).

静(1).

型(G, G).
