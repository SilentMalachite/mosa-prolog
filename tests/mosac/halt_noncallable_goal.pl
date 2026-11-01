:- initialization(main, main).

% レビュー M15。呼べない部分のある catch の Goal は halt/1 を動かす前に型エラーになる。
main :- catch((halt(6), 1), error(type_error(callable, _), _), true).
