% bench/queens.pl の正しさの確かめと短い bench(N) を、テストと差分テスト（mosatest --diff tests/pl）の素材にする（フェーズ6 Task 13）。
% initialization(main, main) はテストの読み込みでは覚えるだけで呼ばれない。
:- include('../../bench/queens').

test(bench_queens_check, check, true).
test(bench_queens_first, once(queens(8, Qs)), solutions([Qs = [4, 2, 7, 3, 6, 8, 5, 1]])).
test(bench_queens_four, queens(4, Qs), solutions([Qs = [3, 1, 4, 2], Qs = [2, 4, 1, 3]])).
test(bench_queens_loop, bench(2), true).
