% library(dcg)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_dcg.pl。ADR 0024 改訂 3。
% 実体は隠し述語 '$dcg_translate'/2。文法規則 H --> B を 1 つ訳し、規則でない項はそのまま返す。
:- module(dcg, [expand_term/2]).

expand_term(Term, Expanded) :- '$dcg_translate'(Term, Expanded).
