% M10 の Codex レビュー。翻訳のときの読み込みは一般の指示を呼ばないので、その op/3 を知らずに次の節を規則
% (p + q) :- 1 と読む。起動時の読みでは事実 +(p, (q :- 1)) なので、翻訳で断らず、起動時の読み込みに任せる。
:- true, op(1200, xfy, +).
p + q :- 1.
:- initialization(main, main).
main :- X = (p + q :- 1), call(X), write(ok), nl.
