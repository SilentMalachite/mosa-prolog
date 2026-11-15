% modules.pl の rx_two_ の行。rx_two だけを取り込む。取り込み元は、再公開が何段あっても定義したモジュールになる。
:- module(rx_two_user, []).
:- use_module(rx_two).
