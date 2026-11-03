% レビュー M10（iso-notes 7.4 の決定 A）。組込述語の節は翻訳のときの読み込みで断り、.cpp を書かない（mosac_emit_refused）。
:- initialization(main, main).

main :- atom_length(abc, L), write(L), nl.

atom_length(_, 99).
