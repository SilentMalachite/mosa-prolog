% bench/nrev.pl の正しさの確かめと短い bench(N) を、テストと差分テスト（mosatest --diff tests/pl）の素材にする（フェーズ6 Task 13）。
% initialization(main, main) はテストの読み込みでは覚えるだけで呼ばれない。
:- include('../../bench/nrev').

test(bench_nrev_check, check, true).
test(bench_nrev_first, (range(1, 5, L), nrev(L, R)), solutions([(L = [1, 2, 3, 4, 5], R = [5, 4, 3, 2, 1])])).
test(bench_nrev_loop, bench(3), true).
