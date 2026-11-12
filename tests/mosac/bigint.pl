% フェーズ10 タスク 10（ADR 0025）。mosac が Big の定数（頭部と本体）と第1引数の鍵を生成コードに書く（mosac_emit_bigint）。
% index_big/2 は頭部に Big を持つ節が 8 つ以上あるので、インタプリタの索引（kIndexMinClauses）と生成コードの鍵の
% 比べ（key_test）の両方を通る。2^63 と -(2^63)-1 は int64 のすぐ外、-(2^63) は int64 に入るので IntBox のまま。
% 最後の節は鍵を持たないので、どの呼び出しの答えにも last が残る。
:- initialization(main, main).

index_big(1267650600228229401496703205376, a0).
index_big(1267650600228229401496703205377, a1).
index_big(1267650600228229401496703205376, a2).
index_big(1267650600228229401496703205377, a3).
index_big(1267650600228229401496703205376, a4).
index_big(1267650600228229401496703205377, a5).
index_big(-1267650600228229401496703205376, a6).
index_big(9223372036854775808, a7).
index_big(-9223372036854775809, a8).
index_big(-9223372036854775808, a9).
index_big(_, last).

keys(K, Vs) :- findall(V, index_big(K, V), Vs).

% 本体で Big を作り、is/2 に渡す。
square(Y) :- X = 123456789012345678901234567890, Y is X * X.

% Big を返す事実。2^64 は 2 語、-(2^128)-1 は 3 語で、真ん中の語が 0 になる。
boundary(9223372036854775808).
boundary(-9223372036854775809).
boundary(18446744073709551616).
boundary(-340282366920938463463374607431768211457).

% 頭部の入れ子の Big は、呼び手の項と照らす枝と、未束縛の呼び手に組み立てる枝の両方を通る。
nested(f([1267650600228229401496703205376, -9223372036854775809]), ok).

% 1 つの節に同じ Big が 2 回出る。
twice(1267650600228229401496703205376, 1267650600228229401496703205376).

wrap(W) :- W = w(18446744073709551616, -18446744073709551616).

row(Name, Value) :- writeq(Name = Value), nl.

main :-
    keys(1267650600228229401496703205376, L1), row(p100, L1),
    keys(1267650600228229401496703205377, L2), row(p100_1, L2),
    keys(-1267650600228229401496703205376, L3), row(neg_p100, L3),
    keys(9223372036854775808, L4), row(p63, L4),
    keys(-9223372036854775809, L5), row(neg_p63_1, L5),
    keys(-9223372036854775808, L6), row(min_int, L6),
    keys(4611686018427387904, L7), row(p62, L7),
    keys(1.0e30, L8), row(float, L8),
    B is 1 << 100, C is B + 1, keys(B, L9), keys(C, L10), row(computed, L9-L10),
    findall(K, (index_big(K, V), V \== last), Ks), row(heads, Ks),
    square(S), row(square, S),
    findall(X, boundary(X), Bs), row(boundary, Bs),
    ( boundary(18446744073709551616) -> row(boundary_2p64, yes) ; row(boundary_2p64, no) ),
    nested(f([A, N]), R), row(nested_out, [A, N, R]),
    ( nested(f([1267650600228229401496703205377, _]), _) -> row(nested_other, yes) ; row(nested_other, no) ),
    nested(T, _), row(nested_build, T),
    twice(X1, Y1), ( X1 == Y1 -> row(twice, same) ; row(twice, differ) ),
    wrap(W), row(wrap, W),
    ( 1267650600228229401496703205376 > 9223372036854775807 -> row(compare, yes) ; row(compare, no) ),
    % 翻訳のときの未定義の呼び出しの検査（src/codegen/check.cpp）は、M:G の G と \+ の中の Big を述語とみなさない。
    catch(user:123456789012345678901234567890, error(E1, _), true), row(qualified, E1),
    catch(\+ (true, 123456789012345678901234567890), error(E2, _), true), row(negated, E2).
