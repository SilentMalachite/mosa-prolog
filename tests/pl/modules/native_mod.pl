% native.pl が M:G を生成コードで試すためのモジュール。公開していない hidden/1 も M:G なら呼べる。
% color/1 は読み込んだ側（user）のもので、既定の取り込みで引く。
:- module(native_mod, [exported_color/1]).

exported_color(X) :- color(X).

hidden(secret).
