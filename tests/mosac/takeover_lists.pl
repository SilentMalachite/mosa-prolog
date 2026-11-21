% レビュー 2026-11-20 の H-28 の敵対的レビュー A1。library(lists) を読んだ後に、利用者のファイルがモジュール lists を
% 宣言して奪う。lib/lists.pl はもうどのモジュールのファイルでもないが、起動時の最初の指示が読むので同梱が要る。
% 捕まえない例外の文面の表も library(format) から library(lists) を読む。
:- use_module(library(lists)).
:- use_module(takeover_lists_mod).
:- initialization(main, main).
main :- mine(X), write(X), nl, atom_length(_, _).
