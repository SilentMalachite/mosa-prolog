:- initialization(main, main).

% レビュー M4（iso-notes 8.14.3 の決定 A）。生成コードでも op/3 はバックトラック、例外、findall で戻らない。
% 全部残れば 3、一つでも戻れば 4 で終わる。
main :-
    findall(x, op(700, xfx, m4_findall), _),
    ( op(700, xfx, m4_disj), fail ; true ),
    catch((op(700, xfx, m4_catch), throw(e)), e, true),
    ( kept([m4_findall, m4_disj, m4_catch]) -> halt(3) ; halt(4) ).

kept([]).
kept([Name | Names]) :-
    current_op(700, xfx, Name),
    kept(Names).
