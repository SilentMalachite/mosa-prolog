% foo.pl が素のパスで読むモジュール。実行ファイルは library/util.pl を埋め込みから引く。
:- module(util, [util/1]).

util(42).
