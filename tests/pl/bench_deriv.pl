% bench/deriv.pl の正しさの確かめと短い bench(N) を、テストと差分テスト（mosatest --diff tests/pl）の素材にする（フェーズ6 Task 13）。
% initialization(main, main) はテストの読み込みでは覚えるだけで呼ばれない。
:- include('../../bench/deriv').

test(bench_deriv_check, check, true).
test(bench_deriv_log, d(log(log(x)), x, D), solutions([D = 1 / x / log(x)])).
test(bench_deriv_divide, d(x / x, x, D), solutions([D = (1 * x - x * 1) / x ^ 2])).
test(bench_deriv_loop, bench(2), true).
