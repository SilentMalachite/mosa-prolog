% modules.pl の rx_call・rx_call_b・rx_own・rx_op。reexport/1 は rx_base の公開述語と公開の演算子を全部公開する。
:- module(rx_all, [ra_own/1]).
:- reexport(rx_base).

ra_own(0).
