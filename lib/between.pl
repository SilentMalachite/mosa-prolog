% library(between)（SPEC 第5節のライブラリ表）。テストは tests/pl/lib_between.pl。
% 引数をすべて確かめてから束縛する。負の数は iso-notes の Section 5 succ/2 の決定 A。
:- module(between, [between/3, succ/2, plus/3]).

between(Low, High, X) :-
    must_be_integer(Low, between/3),
    must_be_bound(High, between/3),
    must_be_integer_or_inf(High, between/3),
    must_be_integer_or_var(X, between/3),
    (   integer(X)
    ->  X >= Low,
        (   High == inf
        ->  true
        ;   X =< High
        )
    ;   High == inf
    ->  between_inf(Low, X)
    ;   Low =< High,
        between_(Low, High, X)
    ).

% 最後の値で選択点を残さないよう、上限に着いたかを先に見る。
between_(Low, High, X) :-
    (   Low =:= High
    ->  X = Low
    ;   (   X = Low
        ;   Next is Low + 1,
            between_(Next, High, X)
        )
    ).

between_inf(Low, X) :-
    (   X = Low
    ;   Next is Low + 1,
        between_inf(Next, X)
    ).

succ(X, Y) :-
    must_be_natural_or_var(X, succ/2),
    must_be_natural_or_var(Y, succ/2),
    (   integer(X)
    ->  Y0 is X + 1,
        Y = Y0
    ;   integer(Y)
    ->  Y > 0,
        X is Y - 1
    ;   throw(error(instantiation_error, context(succ/2, _)))
    ).

% 型を先に見るので、plus(_, 1, a) は具体化の誤りより型の誤りになる。
plus(X, Y, Z) :-
    must_be_integer_or_var(X, plus/3),
    must_be_integer_or_var(Y, plus/3),
    must_be_integer_or_var(Z, plus/3),
    (   integer(X), integer(Y)
    ->  Z0 is X + Y,
        Z = Z0
    ;   integer(X), integer(Z)
    ->  Y is Z - X
    ;   integer(Y), integer(Z)
    ->  X is Z - Y
    ;   throw(error(instantiation_error, context(plus/3, _)))
    ).

must_be_bound(V, PI) :-
    (   var(V)
    ->  throw(error(instantiation_error, context(PI, _)))
    ;   true
    ).

must_be_integer(V, PI) :-
    must_be_bound(V, PI),
    must_be_integer_or_var(V, PI).

must_be_integer_or_var(V, PI) :-
    (   var(V)
    ->  true
    ;   integer(V)
    ->  true
    ;   throw(error(type_error(integer, V), context(PI, _)))
    ).

must_be_integer_or_inf(V, PI) :-
    (   V == inf
    ->  true
    ;   must_be_integer_or_var(V, PI)
    ).

must_be_natural_or_var(V, PI) :-
    must_be_integer_or_var(V, PI),
    (   integer(V), V < 0
    ->  throw(error(domain_error(not_less_than_zero, V), context(PI, _)))
    ;   true
    ).
