% garbage_collect/0、statistics/2、旗 trace_gc（非 ISO、計画 Task 7）。数値は環境で変わるので、関係だけを確かめる。
% 差分テストは答えの束縛まで両側で比べる。実行ごとに変わる数値が答えに残らないよう、確かめる述語に閉じ込める。
gc_loop(0) :- !.
gc_loop(N) :- N1 is N - 1, gc_loop(N1).

gc_counts_up :- statistics(garbage_collection, [C0|_]), garbage_collect, statistics(garbage_collection, [C1|_]), C1 > C0.
gc_plateau :- statistics(heap, [_, Cap0, _]), gc_loop(1000000), statistics(heap, [_, Cap1, _]), Cap1 =:= Cap0.
% Big（ADR 0025）を毎回作って捨てる（フェーズ10 タスク 11）。容量が変わらなければ、GC が捨てた箱を回収している。
% 作った Big を次の回で読み、最初の K はループの後で読む。GC が箱に印を付け損ねるか番地を移し損ねると、詰めた枠や
% 新しい箱が古い番地に入って差が合わなくなる。既定の閾値では K が、ストレス 1 では前の回の Big がそれを見つける。
big_loop(0, _) :- !.
big_loop(N, P) :- X is 2^200 + N, P - X =:= 1, N1 is N - 1, big_loop(N1, X).
big_plateau :- statistics(heap, [_, Cap0, _]), K is 2^200 + 1000001, big_loop(1000000, K), K - 2^200 =:= 1000001,
  statistics(heap, [_, Cap1, _]), Cap1 =:= Cap0.
gc_heap_shape :- statistics(heap, [U, C, L]), integer(U), U =< C, C =< L.
gc_runtime :- statistics(runtime, [T, D]), integer(T), integer(D).
gc_walltime :- statistics(walltime, [T, _]), T >= 0.
gc_cputime :- statistics(cputime, S), float(S).
% 旗を true にした間は GC のたびに標準エラーへ 1 行出る。ストレス 1 では次の安全点で必ず GC が回り、行の数値（KiB と ms）が
% インタプリタと翻訳で食い違うので、true にする往復はここでは試さない。true にして 1 行出ることは tests/cpp/gc_test.cpp。
gc_flag_roundtrip :- current_prolog_flag(trace_gc, false), set_prolog_flag(trace_gc, false), current_prolog_flag(trace_gc, false).

test(gc_collect, garbage_collect, true).
test(gc_counts, gc_counts_up, true).
test(gc_plateau, gc_plateau, true).
test(gc_big_plateau, big_plateau, true).
test(gc_heap_shape, gc_heap_shape, true).
test(gc_runtime, gc_runtime, true).
test(gc_walltime, gc_walltime, true).
test(gc_cputime, gc_cputime, true).
test(gc_key_var, statistics(_, _), error(instantiation_error)).
test(gc_key_type, statistics(1, _), error(type_error(atom, 1))).
test(gc_key_domain, statistics(nope, _), error(domain_error(statistics_key, nope))).
test(gc_flag, gc_flag_roundtrip, true).
test(gc_flag_bad, set_prolog_flag(trace_gc, maybe), error(domain_error(flag_value, trace_gc+maybe))).
