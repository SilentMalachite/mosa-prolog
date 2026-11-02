% レビュー M6（ADR 0010 の改訂 2）。findall が外に持つ答えも上限に数える（mosac_heap_limit の原文）。
% 16 MiB では f(X,X,X) の答え 4 万から 5 万個で止まる。組み直したリストは 600,000 語ほどなので、数えなければ通っていた。
btw(L, H, L) :- L =< H.
btw(L, H, X) :- L < H, L1 is L + 1, btw(L1, H, X).
answers :- findall(f(X, X, X), btw(1, 100000, X), L), length(L, N), write(N), nl.
