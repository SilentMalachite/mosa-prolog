% SPEC 第5節の宣言の例を全部持ち、modules.pl の試験用の述語を足したもの。modules.pl が読む。
:- module(家系, [親/2, 祖父/2, (は)/2, 記録/1, 未定義/1, 全員/2, 全部秘密/1]).
:- use_module(library(lists)).
:- use_module(人名, [姓名分解/3 as 分解]).
:- use_module(人名, except([読み/2])).
:- meta_predicate 全員(1, ?).
:- export(新述語/1).

親(太郎, 一郎).
親(一郎, 花子).
親(一郎, 次郎).

祖父(X, Z) :- 親(X, Y), 親(Y, Z).

は(X, Y) :- 親(Y, X).

新述語(1).

内部(秘密).

名前(家系).

投げる :- throw(家系の例外).

% 取り込んだ側の assertz(記録(X)) は、取り込みをたどってここへ足す。
:- dynamic(記録/1).

% 複.pl が節を足す。自分の節は読み直しても消さない。
:- multifile(複/1).
複(0).

% 受けた引数をそのまま返す。meta_predicate が付ける限定を見るため。
全員(G, G).

% 非公開の 秘密か/1 を apply の述語へ渡す。限定が無ければ apply では見えない。
:- use_module(library(apply)).
全部秘密(L) :- maplist(秘密か, L).
秘密か(秘密).
