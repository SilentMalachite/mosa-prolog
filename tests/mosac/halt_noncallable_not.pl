:- initialization(main, main).

% レビュー M16。呼べない部分のある \+、once/1、forall/2 のゴールは halt/1 を動かす前に型エラーになる。
main :-
    catch(\+ (halt(6), 1), error(type_error(callable, _), _), true),
    catch(once((halt(7), 1)), error(type_error(callable, _), _), true),
    catch(forall((halt(8), 1), true), error(type_error(callable, _), _), true),
    catch(forall(true, (halt(9), 1)), error(type_error(callable, _), _), true).
