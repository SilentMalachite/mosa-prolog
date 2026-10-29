% bench/zebra.pl の正しさの確かめと短い bench(N) を、テストと差分テスト（mosatest --diff tests/pl）の素材にする（フェーズ6 Task 13）。
% initialization(main, main) はテストの読み込みでは覚えるだけで呼ばれない。
:- include('../../bench/zebra').

test(bench_zebra_check, check, true).
test(bench_zebra_owner, (zebra(Hs), mem(house(_, Who, zebra, _, _), Hs), mem(house(_, Drinker, _, water, _), Hs)),
     solutions([(Who = japanese, Drinker = norwegian)])).
test(bench_zebra_loop, bench(2), true).
