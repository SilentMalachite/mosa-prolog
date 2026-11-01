:- initialization(main, main).

% レビュー M5。close を書かずに halt(7) で終わっても、書いた中身はファイルに残る。
main :- open('m5_unclosed_halt.txt', write, S), write(S, halted), halt(7).
