% modules.pl の rx_cycle。再公開の輪の一方。rx_cyc_all を取り込み、その rx_cyc_all はこのモジュールを再公開する。
% このファイルを先に読むと、rx_cyc_all から来る rcx_p/1 の取り込み元はこのモジュール自身になる。
:- module(rx_cyc_x, [rcx_p/1]).
:- use_module(rx_cyc_all).

rcx_p(1).
