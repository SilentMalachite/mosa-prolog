% レビュー 2026-11-20 の H-27（ADR 0030）。前は単位の名前をモジュール名から作ったので、ここで読む 6 つのモジュールは
% どれもビルドを壊した。utable は名前空間 mutable、ain と 'AIN' は入口と同じ main.cpp（大文字小文字を区別しない
% ファイルシステム）、foo と 'Foo' は 1 つのファイル、漢字 30 字は 255 バイトを超えるファイル名になった。
:- use_module(names_utable).
:- use_module(names_ain).
:- use_module(names_ain_upper).
:- use_module(names_foo).
:- use_module(names_foo_upper).
:- use_module(names_kanji).
:- initialization(main, main).

main :- u(A), write(A), nl, a(B), write(B), nl, f(C), g(D), write(C-D), nl, b(E), write(E), nl, k(F), write(F), nl.
