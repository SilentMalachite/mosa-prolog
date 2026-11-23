% 中の consult が初期化の例外で節を残して失敗しても、user:op/3 は節と同じく残る（レビュー L-22）。
:- module(keptops, [kept/0]).
:- user:op(700, xfx, ===>).
kept.
:- ['keptops_init.pl'].
