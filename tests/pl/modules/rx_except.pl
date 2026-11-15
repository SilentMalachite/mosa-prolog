% modules.pl が rx_all の後で読む。rb_a/1 と ===> は rx_all からも来るが、定義は同じ rx_base なので衝突でない。
:- module(rx_except, []).
:- reexport(rx_base, except([rb_b/1])).
