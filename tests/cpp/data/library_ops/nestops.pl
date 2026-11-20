% ゴールの consult は相対パスを作業ディレクトリから開く（iso-notes 7.4.2）。ctest の library_ops はリポジトリの根で動く。
:- module(nestops, []).
:- ( consult('tests/cpp/data/library_ops/badops.pl') -> true ; true ).
