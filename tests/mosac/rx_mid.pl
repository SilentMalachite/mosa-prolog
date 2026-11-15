% standalone の CASE reexport。自分では何も定義せず、rx_lib の lib_p/1 を再公開する。
:- module(rx_mid, []).
:- reexport(rx_lib).
