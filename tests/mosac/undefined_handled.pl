:- initialization(main, main).

% レビュー L12。未定義の述語を呼ぶが、catch で existence_error を捕まえるか unknown を fail にして扱うプログラム。
% mosac は警告を出して作り、実行ファイルは mosa と同じく動く。
main :-
    catch(nosuch, error(existence_error(procedure, nosuch/0), _), write(caught)), nl,
    set_prolog_flag(unknown, fail),
    ( nosuch2 -> true ; write(failed) ), nl.
