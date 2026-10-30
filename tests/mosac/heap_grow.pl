% 生きたリストを伸ばし続ける。ヒープの上限に当たるまで止まらない（mosac_heap_limit の原文）。
main :- grow(0, []).
grow(N, L) :- N1 is N + 1, grow(N1, [N|L]).
