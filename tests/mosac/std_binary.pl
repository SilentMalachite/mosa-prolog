:- initialization(main, main).

% レビュー L8（iso-notes「7.10.2 と 8.12」の決定 A）。標準入力をバイトのとおりに読む。CR も 0x1A もただの字で、
% 標準出力と標準エラーの nl は LF だけを書く。
codes(Codes) :-
    get_code(Code),
    (   Code =:= -1
    ->  Codes = []
    ;   Codes = [Code|Rest],
        codes(Rest)
    ).

main :- codes(Codes), write(Codes), nl, write(user_error, done), nl(user_error).
