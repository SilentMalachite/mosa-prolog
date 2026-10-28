:- module(between, [between/3, succ/2, plus/3]).

% 比較や is/2 の前に見る。そのまま渡すと type_error(evaluable) になる。
bound_int(N) :- var(N), !, throw(error(instantiation_error, _)).
bound_int(N) :- integer(N), !.
bound_int(N) :- throw(error(type_error(integer, N), _)).

% 値の型は範囲より先。空の範囲でも原子は失敗にしない。
between(Low, High, Value) :-
    bound_int(Low),
    bound_int(High),
    ( var(Value) -> between_from(Low, High, Value)
    ; bound_int(Value), Value >= Low, Value =< High
    ).

% 上限を超えてから比べると再帰が終わらない。先に止める。
between_from(Low, High, Low) :- Low =< High.
between_from(Low, High, Value) :-
    Low < High,
    Next is Low + 1,
    between_from(Next, High, Value).

% 0 の前は無い。負は domain、整数でない引数は型の方が先。
succ(A, B) :- var(A), !, bound_int(B), succ_down(B, A).
succ(A, B) :- bound_int(A), ( var(B) -> true ; bound_int(B) ), succ_up(A, B).

succ_down(B, _) :- B < 0, !, throw(error(domain_error(not_less_than_zero, B), _)).
succ_down(B, A) :- B > 0, A is B - 1.

succ_up(A, _) :- A < 0, !, throw(error(domain_error(not_less_than_zero, A), _)).
succ_up(_, B) :- nonvar(B), B < 0, !, throw(error(domain_error(not_less_than_zero, B), _)).
succ_up(A, B) :- B is A + 1.

% 束縛が二つ無いときは instantiation。ある側だけ型を見る。
plus(A, B, C) :- var(A), !, bound_int(B), bound_int(C), A is C - B.
plus(A, B, C) :- var(B), !, bound_int(A), bound_int(C), B is C - A.
plus(A, B, C) :-
    bound_int(A),
    bound_int(B),
    ( var(C) -> C is A + B ; bound_int(C), C =:= A + B ).
