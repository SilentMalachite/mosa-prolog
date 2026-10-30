% garbage_collect/0、statistics/2、旗 trace_gc（非 ISO、計画 Task 7）。数値は環境で変わるので、関係だけを確かめる。
% 差分テストは答えの束縛まで両側で比べる。実行ごとに変わる数値が答えに残らないよう、確かめる述語に閉じ込める。
gc_loop(0) :- !.
gc_loop(N) :- N1 is N - 1, gc_loop(N1).

gc_counts_up :- statistics(garbage_collection, [C0|_]), garbage_collect, statistics(garbage_collection, [C1|_]), C1 > C0.
gc_plateau :- statistics(heap, [_, Cap0, _]), gc_loop(1000000), statistics(heap, [_, Cap1, _]), Cap1 =:= Cap0.
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
test(gc_heap_shape, gc_heap_shape, true).
test(gc_runtime, gc_runtime, true).
test(gc_walltime, gc_walltime, true).
test(gc_cputime, gc_cputime, true).
test(gc_key_var, statistics(_, _), error(instantiation_error)).
test(gc_key_type, statistics(1, _), error(type_error(atom, 1))).
test(gc_key_domain, statistics(nope, _), error(domain_error(statistics_key, nope))).
test(gc_flag, gc_flag_roundtrip, true).
test(gc_flag_bad, set_prolog_flag(trace_gc, maybe), error(domain_error(flag_value, trace_gc+maybe))).
