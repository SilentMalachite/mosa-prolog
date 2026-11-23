% レビュー R-M03 と R-M04（transcript_debug_spy_alias_two）。q は alias2_m:p/1、p は alias2_m:other/1 の別名である。
% alias2_c は alias2_m2 の p/1 を q として取り込むが、alias2_m2 はまだ p/1 を定義していない。
:- use_module('alias2_m.pl', [p/1 as q, other/1 as p]).
:- use_module('alias2_c.pl', []).
