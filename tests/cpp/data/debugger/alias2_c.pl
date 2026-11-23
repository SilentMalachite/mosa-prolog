% レビュー R-M03。alias2_m2 の p/1 を q として取り込む。読んだ時には p/1 の節が無い。
:- module(alias2_c, []).
:- use_module('alias2_m2.pl', [p/1 as q]).
