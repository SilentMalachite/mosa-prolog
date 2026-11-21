% レビュー 2026-11-20 の H-28 の敵対的レビュー A5。MOSA_LIB に置く独自の文面の表。library(apply) は節の本体だけが読む。
% 実行ファイルは捕まえない例外の文面を作るときにこの節を動かすので、apply も同梱が要る。
:- module(messages_ja, []).
:- use_module(library(format)).

message(goal_raised, Format) :- use_module(library(apply)), apply:maplist(=(Format), ['独自の表: ~q']).
