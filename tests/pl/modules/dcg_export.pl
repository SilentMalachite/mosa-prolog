% 文法規則の非終端記号を Name//Arity で書き出す（ADR 0024 改訂 3）。
:- module(dcg_export, [greet//0, dcg_item//1]).
greet --> [hello], dcg_item(world).
dcg_item(X) --> [X].
