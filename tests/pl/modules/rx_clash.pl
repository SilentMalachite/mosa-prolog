% modules.pl の rx_conflict。user が rx_base から取り込んだ rb_a/1 とは違う述語を同じ名前で公開する。
:- module(rx_clash, [rb_a/1]).

rb_a(clash).
