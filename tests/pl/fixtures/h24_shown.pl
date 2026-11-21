% レビュー 2026-11-20 の H-24。portray/1 を輸出するモジュール。user が取り込めば、user から呼ぶ portray/1 になる。
:- module(h24_shown, [portray/1]).
portray(h24_me) :- write(seen).
