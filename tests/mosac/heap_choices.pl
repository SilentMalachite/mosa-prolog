% レビュー M17（ADR 0010 の改訂 3）。選択点、継続、env も上限に数える（mosac_heap_limit の原文）。
% 1 歩ごとに選択点を 1 つ残し、1 万歩ごとに歩数を書く。16 MiB では 10 万歩より前に止まる。数えなければ、
% ヒープが上限に着くまで（mosa で約 100 万歩）進んだ。
deep(N) :- ( N mod 10000 =:= 0 -> write(N), nl ; true ), N1 is N + 1, ( deep(N1) ; true ).
choices :- deep(0).
