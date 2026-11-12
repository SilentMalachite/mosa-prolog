% フェーズ10（ADR 0025）。int64 に入らない整数（Big）のテスト。字句解析、reader、number_codes/2 の族が字面から
% Big を作る。is や =:= はタスク 6 まで Big で int_overflow になるので、値は == で比べる。
test(lit_big, (X = 123456789012345678901234567890, Y = 123456789012345678901234567890, X == Y), true).
test(lit_distinct, (X = 123456789012345678901234567890, Y = 123456789012345678901234567891, X \== Y), true).
test(lit_hex, (X = 0xffffffffffffffffffff, X == 1208925819614629174706175), true).
test(lit_sep, (X = 1_000_000_000_000_000_000_000, X == 1000000000000000000000), true).
% -2^63 は int64 に入るので、Big ではなく min_integer と同じ IntBox になる（ADR 0025 の正規形）。
test(neg_min, (X = -9223372036854775808, current_prolog_flag(min_integer, M), X == M), true).
test(nc_big, (number_codes(X, "-1267650600228229401496703205376"), number_codes(X, C), atom_codes(A, C), A == '-1267650600228229401496703205376'), true).
% 負の Big を演算子の引数に書いた字面は、読み戻すと元の項になる（7.10.5）。
test(wq_neg_1, (X = -(-1267650600228229401496703205376), term_to_atom(X, A), term_to_atom(Y, A), Y == X), true).
test(wq_neg_2, (X = 1 - (-1267650600228229401496703205376), term_to_atom(X, A), term_to_atom(Y, A), Y == X), true).
test(wq_neg_3, (X = -(1267650600228229401496703205376), Y = -1267650600228229401496703205376, term_to_atom(X, A), term_to_atom(Y, B), A \== B, term_to_atom(X1, A), X1 == X, term_to_atom(Y1, B), Y1 == Y), true).
test(atom_number_big, (atom_number('18446744073709551616', N), N == 18446744073709551616), true).
