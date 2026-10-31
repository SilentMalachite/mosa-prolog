% ADR 0012（mosac_heap_limit の原文）。決定的に抜けた catch/3 と *-> の Entry は残らないので、1 MiB の上限でも
% 1,000,000 段まわる。外さなければ Entry の根でヒープが埋まり、上限で止まる。
lw(0) :- !.
lw(N) :- catch(true, _, true), N1 is N - 1, lw(N1).
ls(0) :- !.
ls(N) :- (true *-> true ; true), N1 is N - 1, ls(N1).
exits :- lw(1000000), ls(1000000), write(exits_done), nl.
