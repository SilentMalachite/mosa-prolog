% レビュー 2026-11-20 の H-28 の敵対的レビュー A1。文面の表と関わらない形。library(apply) を読んだ後に、利用者のファイルが
% モジュール apply を宣言して奪う。lib/apply.pl は起動時の最初の指示が読むので同梱が要る。
:- use_module(library(apply)).
:- use_module(takeover_apply_mod).
:- initialization(main, main).
main :- mine(X), write(X), nl.
