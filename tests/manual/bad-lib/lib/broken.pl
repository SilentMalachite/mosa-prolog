% 構文の誤りを持つライブラリ（tests/manual/bad-lib、manual_fixture_bad_lib と manual_parse_test）。
% mosatest --manual は読めないライブラリとして、名前とパスと読み込みの誤りを出す。
:- module(broken, [broken/0]).

broken :- ).
