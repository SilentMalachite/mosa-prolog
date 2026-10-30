% 1 MiB（2^17 語）の上限での物差しと、上限の近くで空きを生まない GC（mosac_heap_limit の原文、ADR 0010 の改訂 B）。
% yardsticks は生きている項が小さいので最後まで走る。thrash は length/2 で上限の 96% を生かしたまま捨てる項を
% 作り続け、閾値で始まる GC が上限の 1/32 を空けられないので、3 回で上限のメッセージを出して止まる。
loop(0) :- !.
loop(N) :- N1 is N - 1, loop(N1).
tak(X, Y, Z, A) :- X =< Y, !, Z = A.
tak(X, Y, Z, A) :- X1 is X - 1, Y1 is Y - 1, Z1 is Z - 1,
  tak(X1, Y, Z, A1), tak(Y1, Z, X, A2), tak(Z1, X, Y, A3), tak(A1, A2, A3, A).
yardsticks :- loop(1000000), tak(18, 12, 6, A), write(tak(A)), nl.
junk(0) :- !.
junk(K) :- _ = f(K, K, K, K), K1 is K - 1, junk(K1).
grow(N, L) :- junk(5), N1 is N + 1, grow(N1, [N|L]).
thrash :- length(L, 42000), grow(0, L).
