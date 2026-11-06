% レビュー M28 の Codex 敵対的レビュー。別名 '.' の頭はリストのセルになる。modules.pl が読む。
:- module(m28_dot_back, [m28_back_meta/1, m28_back_call/1]).
:- use_module('m28_dot_src.pl', [m28_r/2 as '.']).

m28_back_meta(M) :- predicate_property('.'(_, _), meta_predicate(M)).
m28_back_call(X) :- '.'(X = 1, _).
