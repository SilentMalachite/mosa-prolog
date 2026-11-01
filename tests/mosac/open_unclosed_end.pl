:- initialization(main, main).

% レビュー M5。close も halt も書かずに main が終わっても、書いた中身はファイルに残る。
main :- open('m5_unclosed_end.txt', write, S), write(S, ended), nl(S).
