% use_module/2 の as で別名を付けて取り込む。modules.pl が読む。
:- module(別名, []).
:- use_module('人名.pl', [姓名分解/3 as 分解]).
