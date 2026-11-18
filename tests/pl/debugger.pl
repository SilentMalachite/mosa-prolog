% フェーズ14 タスク 4: デバッガの 9 つの述語（13211-1 に無い。ADR 0028）。mosatest には口が無くポートを出さないので、
% 状態と誤りだけを試す。ポートの行と知らせの字面は tests/cpp/data/debugger のトランスクリプトで見る。
% どの行も終わりに nodebug, nospyall, leash(all) で状態を戻す。
dbg_p(1).
dbg_q.

test(dbg_trace_on, (trace, current_prolog_flag(debug, on), nodebug, nospyall, leash(all)), true).
test(dbg_notrace_keeps, (trace, notrace, current_prolog_flag(debug, on), nodebug, nospyall, leash(all)), true).
test(dbg_debug_on, (debug, current_prolog_flag(debug, on), nodebug, nospyall, leash(all)), true).
test(dbg_nodebug_off, (debug, nodebug, current_prolog_flag(debug, off), nospyall, leash(all)), true).
test(dbg_flag_off, (trace, set_prolog_flag(debug, off), current_prolog_flag(debug, off), nodebug, nospyall, leash(all)), true).
test(dbg_flag_on, (set_prolog_flag(debug, on), current_prolog_flag(debug, on), nodebug, nospyall, leash(all)), true).
test(dbg_spy_debug, (spy(dbg_p/1), current_prolog_flag(debug, on), nodebug, nospyall, leash(all)), true).
test(dbg_spy_undefined, (spy(dbg_nowhere/3), nodebug, nospyall, leash(all)), true).
test(dbg_spy_name, (spy(dbg_p), nodebug, nospyall, leash(all)), true).
test(dbg_spy_name_none, (spy(dbg_no_such), nodebug, nospyall, leash(all)), true).
% 何も置かない spy/1 はデバッガを入れない。ポートの無い述語にも置かない（PR #4 の敵対的レビュー、既定 A）。
test(dbg_spy_portless_off, (nodebug, spy(true/0), current_prolog_flag(debug, off), nospyall, leash(all)), true).
test(dbg_spy_call_off, (nodebug, spy(call), current_prolog_flag(debug, off), nospyall, leash(all)), true).
test(dbg_spy_empty_off, (nodebug, spy([]), current_prolog_flag(debug, off), nospyall, leash(all)), true).
test(dbg_spy_none_off, (nodebug, spy(dbg_no_such), current_prolog_flag(debug, off), nospyall, leash(all)), true).
test(dbg_spy_list, (spy([dbg_p/1, dbg_q/0]), nodebug, nospyall, leash(all)), true).
test(dbg_spy_qualified, (spy(user:dbg_p/1), nodebug, nospyall, leash(all)), true).
test(dbg_spy_builtin, (spy(append/3), spy(atom_length/2), nodebug, nospyall, leash(all)), true).
test(dbg_nospy, (spy(dbg_p/1), nospy(dbg_p/1), nodebug, nospyall, leash(all)), true).
test(dbg_nospy_name, (spy(dbg_p), nospy(dbg_p), nodebug, nospyall, leash(all)), true).
test(dbg_nospy_absent, (nospy(dbg_q/0), nodebug, nospyall, leash(all)), true).
test(dbg_nospyall, (spy(dbg_p/1), nospyall, nodebug, leash(all)), true).
test(dbg_leash_forms, (leash(all), leash(none), leash([call, -exit, +fail]), leash(-redo), leash(call), leash(+exit), leash([]), nodebug, nospyall, leash(all)), true).
test(dbg_debugging, (debugging, nodebug, nospyall, leash(all)), true).
test(dbg_debugging_on, (trace, spy(dbg_p/1), leash([call, fail]), debugging, nodebug, nospyall, leash(all)), true).
test(dbg_trace_then_nodebug_spy, (trace, nodebug, spy(dbg_p/1), nodebug, nospyall, leash(all)), true).

test(dbg_spy_inst, spy(_), error(instantiation_error)).
test(dbg_spy_type, spy(1), error(type_error(predicate_indicator, 1))).
test(dbg_spy_arity_inst, spy(dbg_p/_), error(instantiation_error)).
test(dbg_spy_arity_type, spy(dbg_p/a), error(type_error(integer, a))).
test(dbg_spy_arity_neg, spy(dbg_p/(-1)), error(domain_error(not_less_than_zero, -1))).
test(dbg_spy_name_type, spy(1/2), error(type_error(atom, 1))).
% 後始末は catch の外に置く。誤りで終わる形では後段が走らず、部分適用が次の行へ漏れても検出も掃除もできないため。
test(dbg_spy_list_bad, (catch(spy([dbg_p/1, 1]), E, true), E = error(type_error(predicate_indicator, 1), _), current_prolog_flag(debug, off), nodebug, nospyall), true).
test(dbg_spy_list_partial, spy([dbg_p/1|_]), error(instantiation_error)).
test(dbg_nospy_inst, nospy(_), error(instantiation_error)).
test(dbg_leash_inst, leash(_), error(instantiation_error)).
test(dbg_leash_domain, leash(foo), error(domain_error(port, foo))).
test(dbg_leash_plus_domain, leash(+foo), error(domain_error(port, foo))).
test(dbg_leash_minus_domain, leash(-foo), error(domain_error(port, foo))).
test(dbg_leash_type, leash(1), error(type_error(atom, 1))).
test(dbg_leash_partial, leash([call|_]), error(instantiation_error)).
test(dbg_leash_item_inst, leash([call, _]), error(instantiation_error)).
% 定義し直しは、system の述語への permission_error で読み込みが止まる（SPEC 第5節）。
% ファイルを開けることを先に見る。開けないファイルの consult も失敗するので、それだけでは区別できない。
test(dbg_redefine, (open('tests/pl/fixtures/dbg_redefine.pl', read, S), close(S), \+ consult('tests/pl/fixtures/dbg_redefine.pl')), true).
% 知らせは user_error の流れを通るので、行の頭に戻る（流れの位置が進む）。
test(dbg_notice_column, (write(user_error, x), debugging, '$stream_column'(user_error, C), nodebug, nospyall, leash(all)), solutions([C = 0])).
% 読み込みが失敗したら、入り切りと進み方は読み込みの前に戻る。進み方は debugging/0 の字面でだけ見える。
test(dbg_rollback_off, (consult('tests/pl/fixtures/dbg_rollback.pl') -> fail ; current_prolog_flag(debug, off)), true).
% 入っていたデバッガを指令が切ってから読み込みが失敗する。戻らなければ off になる。同じ指令だけの読み込みが成功すれば
% off のまま残ることを対照に見る。
test(dbg_rollback_on, (trace, (consult('tests/pl/fixtures/dbg_rollback_nodebug.pl') -> fail ; current_prolog_flag(debug, on)), nodebug, nospyall, leash(all)), true).
test(dbg_rollback_control, (trace, consult('tests/pl/fixtures/dbg_nodebug_ok.pl'), current_prolog_flag(debug, off), nospyall, leash(all)), true).
