% ctest は build が作業ディレクトリ。直接実行はリポジトリの根。
consult_names :- consult('tests/pl/modules/morning/人名.pl').
consult_names :- consult('../tests/pl/modules/morning/人名.pl').

consult_family :- consult('tests/pl/modules/morning/家系.pl').
consult_family :- consult('../tests/pl/modules/morning/家系.pl').

% 確認は、家系を読んだときの use_module が maplist と分解を家系へ入れる。
test(morning_confirm, (consult_names, consult_family, use_module(家系), 確認), true).

% ここからは morning_confirm が載せたモジュールを使う。同じファイルの consult はもう一度失敗する。
test(morning_grand, 祖父(太郎, 三郎), true).
test(morning_lists, (use_module(library(lists)), member(花子, [次郎, 花子])), true).
test(morning_qualified, 家系:祖父(太郎, 三郎), true).
test(morning_hidden, \+ 読み(太郎, たろう), true).
test(morning_qualified_read, 人名:読み(太郎, たろう), true).
% maplist は家系へ入る。user に席が無いと、定義が別モジュールにあっても失敗する。
test(morning_maplist_stays, \+ maplist(親(太郎), [次郎]), true).
