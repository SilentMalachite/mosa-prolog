% native.pl が meta_predicate を生成コードで試すためのモジュール。限定する引数は呼び手の文脈モジュールで M: に包まれる。
:- module(native_meta, [keep_goal/2, kept_inside/1, call_kept/1]).
:- meta_predicate keep_goal(0, -).

% 包んだゴールをそのまま返す。文脈モジュールが答えに出る
keep_goal(G, G).

% このモジュールの中から呼ぶと、文脈は native_meta
kept_inside(Q) :- keep_goal(foo, Q).

% 包んだゴールを呼ぶ。color/1 は native_meta:color(X) として引かれ、既定の取り込みで user のものに届く
call_kept(G) :- keep_goal(G, Q), call(Q).
