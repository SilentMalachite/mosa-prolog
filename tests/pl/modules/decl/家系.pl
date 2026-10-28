% docs/SPEC.md 第5節の宣言例。祖父が三郎になる親を2つ足してある。
:- module(家系, [親/2, 祖父/2, (は)/2]).
:- use_module(library(lists)).
:- use_module(人名, [姓名分解/3 as 分解]).
:- use_module(人名, except([読み/2])).
:- meta_predicate 全員(1, ?).
:- export(新述語/1).

親(太郎, 次郎).
親(次郎, 三郎).
祖父(X, Z) :- 親(X, Y), 親(Y, Z).
