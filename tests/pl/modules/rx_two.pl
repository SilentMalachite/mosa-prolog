% modules.pl の rx_two_ の行。rx_all を再公開する 2 段目。rx_all は rb_a/1 と rb_b/1 を rx_base から再公開する。
:- module(rx_two, []).
:- reexport(rx_all).
