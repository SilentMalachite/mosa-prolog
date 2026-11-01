:- initialization(main, main).

% レビュー M15。呼べない部分のある Recovery は halt/1 を動かす前に型エラーになる。
main :- catch(catch(throw(e), _, (halt(7), 1)), error(type_error(callable, _), _), true).
