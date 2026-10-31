% control.pl の op_exit_same が読む（ADR 0012）。決定的に抜けた catch/3 と *-> の後の op/3 は、once/1 や -> の後と同じに扱う。
:- once(true), op(700, xfx, '=once=>').
:- catch(true, _, true), op(700, xfx, '=catch=>').
:- (true *-> true ; true), op(700, xfx, '=soft=>').
:- (true -> true ; true), op(700, xfx, '=ite=>').
