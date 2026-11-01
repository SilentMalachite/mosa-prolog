:- initialization(main, main).

% レビュー M15。呼べない部分のある catch の Goal と Recovery は何も動かさずに型エラーになるので、中で名指す
% 未定義の述語は呼ばれない。mosac もそれを未定義の呼び出しとして拒まない。_ は 1 回だけ現れる変数。
main :- catch((nosuch_goal, 1), error(type_error(callable, _), _), true),
        catch(catch(throw(e), _, (nosuch_recovery, _, 2.5)), error(type_error(callable, _), _), true).
