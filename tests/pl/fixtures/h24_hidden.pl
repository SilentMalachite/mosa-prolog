% レビュー 2026-11-20 の H-24。portray/1 を輸出しないモジュール。user からは見えないので、print/1 の掛け金にならない。
:- module(h24_hidden, [h24_hello/0]).
portray(_) :- write(hidden).
h24_hello.
