% レビュー M28 の Codex 敵対的レビュー。m28_dot_back.pl が m28_r/2 を '.' の名で取り込む。
:- module(m28_dot_src, [m28_r/2]).
:- meta_predicate m28_r(0, ?).

m28_r(G, _) :- call(G).
