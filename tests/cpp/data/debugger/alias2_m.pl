% レビュー R-M03 と R-M04。p/1、other/1、w/1 を公開する。alias2.pl は p/1 を q、other/1 を p として取り込む。
% w/1 は .in が後で t として取り込む。
:- module(alias2_m, [p/1, other/1, w/1]).
p(1).
other(2).
w(3).
