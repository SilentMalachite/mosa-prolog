% library(dcg)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_dcg.pl。ADR 0024 改訂 3。
% 実体は隠し述語 '$dcg_translate'/2。文法規則 H --> B を 1 つ訳し、規則でない項はそのまま返す。
:- module(dcg, [expand_term/2]).

% 隠し述語の名前を誤りの Context に出さない。lib/format.pl の must_be_list/2 と同じく、Formal を取って公開名で投げ直す。
expand_term(Term, Expanded) :-
    catch('$dcg_translate'(Term, Expanded), error(Formal, _), throw(error(Formal, context(expand_term/2, _)))).
