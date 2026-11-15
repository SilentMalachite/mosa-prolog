% modules.pl の rx_alias・rx_alias_from。PI のリストの reexport/2 は、as の別名で公開する。
:- module(rx_some, []).
:- reexport(rx_base, [rb_a/1 as rs_a]).
