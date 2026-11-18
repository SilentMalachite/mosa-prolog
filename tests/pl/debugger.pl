% フェーズ14 タスク 4: デバッガの 9 つの述語（13211-1 に無い。ADR 0028）。ポートはまだ出さないので、状態と誤りだけを試す。
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
test(dbg_spy_list_bad, (spy([dbg_p/1, 1]), nodebug, nospyall), error(type_error(predicate_indicator, 1))).
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
test(dbg_redefine, consult('tests/pl/fixtures/dbg_redefine.pl'), fail).
