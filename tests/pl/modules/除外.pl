% use_module/2 の except で一部を除いて取り込む。modules.pl が読む。
:- module(除外, []).
:- use_module('人名.pl', except([読み/2])).
