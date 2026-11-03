% 選択点を残して成功した指示は、その選択点を切り、setup_call_cleanup/3 の後始末を動かす（ADR 0016、レビュー M11）。
:- use_module(library(lists)).
:- dynamic(m11_dir_cleanup/1).
:- setup_call_cleanup(true, member(X, [1, 2]), assertz(m11_dir_cleanup(X))).

test(scc_directive_cut, m11_dir_cleanup(X), solutions([X = 1])).
