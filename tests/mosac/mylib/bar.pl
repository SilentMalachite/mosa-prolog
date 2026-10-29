% $MOSA_LIB のライブラリ。下のディレクトリのモジュールを素のパス 'sub/deep' で use_module する（計画 Task 10 直し 2）。
:- module(bar, [bar/1]).
:- use_module('sub/deep').

bar(X) :- deep(X).
