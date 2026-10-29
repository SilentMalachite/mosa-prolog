% bench/tak.pl の正しさの確かめと短い bench(N) を、テストと差分テスト（mosatest --diff tests/pl）の素材にする（フェーズ6 Task 13）。
% initialization(main, main) はテストの読み込みでは覚えるだけで呼ばれない。
:- include('../../bench/tak').

test(bench_tak_check, check, true).
test(bench_tak_small, tak(12, 8, 4, A), solutions([A = 5])).
test(bench_tak_loop, bench(2), true).
