% bench/crypt.pl の正しさの確かめと短い bench(N) を、テストと差分テスト（mosatest --diff tests/pl）の素材にする（フェーズ6 Task 13）。
% initialization(main, main) はテストの読み込みでは覚えるだけで呼ばれない。
:- include('../../bench/crypt').

test(bench_crypt_check, check, true).
test(bench_crypt_all, crypt(Ds, Es, _, _, Ps), solutions([(Ds = [3, 4, 8], Es = [2, 8], Ps = [9, 7, 4, 4])])).
test(bench_crypt_sum, sum([9, 9], [1], Cs), solutions([Cs = [0, 0, 1]])).
test(bench_crypt_loop, bench(2), true).
