% modules.pl の rx_ の行（フェーズ12 Task 6）。rx_all、rx_some、rx_except が再公開する元。===> は公開の演算子。
:- module(rx_base, [rb_a/1, rb_b/1, (===>)/2, op(700, xfx, ===>)]).

rb_a(1).
rb_b(2).
X ===> X.
