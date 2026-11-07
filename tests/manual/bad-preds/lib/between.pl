% $MOSA_LIB の代わり（manual_fixture_bad_preds_mosa_lib）。bad-preds の between の表の行とちょうど同じ述語を
% 公開するので、mosatest --manual が $MOSA_LIB から library(between) を読めば、その表は誤って通る。
:- module(between, [between/3, nope/1, plus/3]).

between(_, _, _).
nope(_).
plus(_, _, _).
