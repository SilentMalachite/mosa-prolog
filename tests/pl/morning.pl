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
% 人名が公開していても user は取り込んでいない。unknown=error では存在エラー。
test(morning_hidden, 読み(太郎, たろう), error(existence_error(procedure, 読み/2))).
test(morning_qualified_read, 人名:読み(太郎, たろう), true).
% maplist は家系へ入る。user の解決順に無い。
test(morning_maplist_stays, maplist(親(太郎), [次郎]), error(existence_error(procedure, maplist/2))).
