% bench/bigint.pl の正しさの確かめと短い bench(N) を、テストと差分テスト（mosatest --diff tests/pl）の素材にする（フェーズ10 タスク 12）。
% initialization(main, main) はテストの読み込みでは覚えるだけで呼ばれない。
:- include('../../bench/bigint').

test(bench_bigint_check, check, true).
test(bench_bigint_fact, fact(25, F), solutions([F = 15511210043330985984000000])).
test(bench_bigint_loop, bench(2), true).
