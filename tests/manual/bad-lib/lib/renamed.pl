% 語幹と違う名前のモジュールを宣言するファイル（tests/manual/bad-lib、manual_fixture_bad_lib と manual_parse_test）。
% 読めるがモジュール renamed は無いので、mosatest --manual は読めないライブラリとして出す。
:- module(other, [renamed/0]).

renamed.
