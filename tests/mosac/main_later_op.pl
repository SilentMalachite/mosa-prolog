% レビュー L23。initialization(G, main) のゴールは、後の op/3 の下でも指令の項のまま動く。mosac の実行ファイルも
% 同じ run_main_goal で終わる。前は = を外すと読めずに 1 で、- の向きを変えると別の項になって失敗した。
:- initialization(check(a = b, 1 - 2 - 3), main).
check(E, D) :- E == (a = b), D == (1 - 2) - 3, write(same), nl.
:- op(0, xfx, =).
:- op(500, xfy, -).
