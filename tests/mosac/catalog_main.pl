% レビュー 2026-11-20 の H-28。文面の表（ADR 0007）が読むモジュールと同じ名前の利用者のモジュール lists、format、
% messages_ja を読む。前は mosac の翻訳がこの 3 つを lib/ の物で置き換えて単位を出さず、3 つは IR のまま動いた。
% 最後のゴールは捕まえない例外を出し、実行ファイルはその文面を同梱の文面の表から作る。
:- use_module(catalog_lists).
:- use_module(catalog_format).
:- use_module(catalog_messages).
:- initialization(main, main).

main :- l(A), f(B), m(C), write(A-B-C), nl, atom_length(_, _).
