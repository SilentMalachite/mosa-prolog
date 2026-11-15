% modules.pl の imp_* の行（フェーズ12 Task 8）。import/1 の取り込み先。自分の rb_a/1 があるので、rx_base の rb_a/1 は衝突になる。
:- module(imp_ctx, []).

rb_a(local).
