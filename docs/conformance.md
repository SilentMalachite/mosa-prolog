このファイルは自動生成である。手で編集しない。
分母: 2172
合格数: 1373
合格率: 63.2
不合格:
control/call_1/tests.lgt iso_call_1_05 error(instantiation_error) suppress_text_output/0
control/call_1/tests.lgt iso_call_1_06 errors([type_error(callable, (write(3), 3)), type_error(callable, 3)]) suppress_text_output/0
control/call_1/tests.lgt iso_call_1_09 error(instantiation_error) suppress_text_output/0
control/call_1/tests.lgt iso_call_1_10 errors([type_error(callable, 1), type_error(callable, user:1)]) unsupported
control/call_1/tests.lgt iso_call_1_12 errors([type_error(callable, 1), type_error(callable, user:1)]) unsupported
control/call_1/tests.lgt iso_call_1_14 errors([type_error(callable, (write(3), 1)), type_error(callable, 1)]) suppress_text_output/0
control/call_1/tests.lgt lgt_call_1_17 false existence_error(procedure, fenix/1)
control/call_1/tests.lgt lgt_call_1_18 false existence_error(procedure, scattered/2)
control/call_1/tests.lgt lgt_call_1_19 errors([existence_error(procedure, foobar/1), existence_error(procedure, user:foobar/1)]) unsupported
control/catch_3/tests.lgt iso_catch_3_07 true(C==c) suppress_text_output/0
control/catch_3/tests.lgt lgt_catch_3_10 true((subsumes_term(error(type_error(callable, 1), _), Y);subsumes_term(error(type_error(callable, user:1), _), Y))) unsupported
control/catch_3/tests.lgt lgt_catch_3_13 true(Assertion) existence_error(procedure, ignore/1)
control/conjunction_2/tests.lgt lgt_conjunction_2_04 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (3, true))]) unsupported
control/conjunction_2/tests.lgt lgt_conjunction_2_05 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (true, 3))]) unsupported
control/cut_0/tests.lgt iso_cut_0_10 true(Assertion) fail
control/cut_0/tests.lgt eclipse_cut_0_19 true(L==[1, 2]) fail
control/disjunction_2/tests.lgt lgt_disjunction_2_06 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (3;true))]) unsupported
control/disjunction_2/tests.lgt lgt_disjunction_2_07 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (fail;3))]) unsupported
control/if_3/tests.lgt if_3_01 true existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_02 true existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_03 false existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_04 false existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_05 true(L==[1-4, 1-5, 1-6, 2-4, 2-5, 2-6, 3-4, 3-5, 3-6]) existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_06 true(L==[7, 8, 9]) existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_07 true(L==[7, 8, 9]) existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_08 true existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_09 true(L==[1, 2]) existence_error(procedure, if/3)
control/if_3/tests.lgt if_3_10 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, if(3, true, true))]) unsupported
control/if_3/tests.lgt if_3_11 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, if(true, 3, true))]) unsupported
control/if_3/tests.lgt if_3_12 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, if(fail, true, 3))]) unsupported
control/if_then_2/tests.lgt lgt_if_then_2_10 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (3->true))]) unsupported
control/if_then_2/tests.lgt lgt_if_then_2_11 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (true->3))]) unsupported
control/if_then_else_3/tests.lgt lgt_if_then_else_3_11 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (3->true;fail))]) unsupported
control/if_then_else_3/tests.lgt lgt_if_then_else_3_12 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (true->3;fail))]) unsupported
control/if_then_else_3/tests.lgt lgt_if_then_else_3_13 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (fail->true;3))]) unsupported
control/soft_cut_2_3/tests.lgt lgt_soft_cut_2_3_18 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (*->(3, true);fail))]) unsupported
control/soft_cut_2_3/tests.lgt lgt_soft_cut_2_3_19 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (*->(true, 3);fail))]) unsupported
control/soft_cut_2_3/tests.lgt lgt_soft_cut_2_3_20 errors([type_error(callable, 3), type_error(callable, user:3), type_error(callable, (*->(fail, true);3))]) unsupported
control/soft_cut_2_3/tests.lgt lgt_soft_cut_2_3_22 true(Deterministic==false) existence_error(procedure, :: /2)
directives/ensure_loaded_1/tests.lgt ensure_loaded_1_01 true(L==[1]) existence_error(procedure, a/1)
directives/ensure_loaded_1/tests.lgt ensure_loaded_1_02 true(L==[2]) existence_error(procedure, b/1)
directives/ensure_loaded_1/tests.lgt ensure_loaded_1_03 true(L==[3]) existence_error(procedure, c/1)
directives/include_1/tests.lgt include_1_01 true(L==[1, 2, 3]) existence_error(procedure, a/1)
directives/initialization_1/tests.lgt ensure_loaded_1_01 true(L==[1]) existence_error(procedure, a/1)
directives/initialization_1/tests.lgt ensure_loaded_1_02 true(L==[2]) existence_error(procedure, b/1)
directives/initialization_1/tests.lgt ensure_loaded_1_03 true(L==[1, 2, 3]) existence_error(procedure, c/1)
functions/acos_1/tests.lgt iso_acos_1_01 true(X=~=3.1415927) fail
functions/acos_1/tests.lgt iso_acos_1_02 true(X=~=1.0471976) fail
functions/asin_1/tests.lgt iso_asin_1_01 true(X=~=0.5235988) fail
functions/asin_1/tests.lgt iso_asin_1_02 true(X=~=3.1415927) fail
functions/atan2_2/tests.lgt iso_atan2_2_01 true(X=~=1.5707963) fail
functions/atan2_2/tests.lgt iso_atan2_2_02 true(X=~=3.1415927) fail
functions/atan_1/tests.lgt iso_atan_1_02 true(PI=~=3.1415927) fail
functions/ceiling_1/tests.lgt lgt_ceiling_1_05 error(type_error(float, 9)) true
functions/cos_1/tests.lgt iso_cos_1_05 true(X=~=0.0) fail
functions/exp_1/tests.lgt iso_exp_1_02 true(X=~=2.71828) fail
functions/float_fractional_part_1/tests.lgt lgt_float_fractional_part_1_02 true(X=~=0.9) fail
functions/float_fractional_part_1/tests.lgt lgt_float_fractional_part_1_04 error(type_error(float, 9)) true
functions/float_integer_part_1/tests.lgt lgt_float_integer_part_1_04 error(type_error(float, 9)) true
functions/floor_1/tests.lgt lgt_floor_1_06 error(type_error(float, 9)) true
functions/integer_power_2/tests.lgt iso_integer_power_2_06 error(type_error(float, 2)) evaluation_error(undefined)
functions/integer_power_2/tests.lgt iso_integer_power_2_09 true(X=~=0.353553) fail
functions/log_1/tests.lgt iso_log_1_02 true(E=~=1.0) fail
functions/log_2/tests.lgt lgt_log_2_01 true(X=~=3.0) type_error(evaluable, log/2)
functions/log_2/tests.lgt lgt_log_2_02 true(X=~=1.0) type_error(evaluable, log/2)
functions/log_2/tests.lgt lgt_log_2_03 true(X=~=1.0) type_error(evaluable, log/2)
functions/log_2/tests.lgt lgt_log_2_12 error(evaluation_error(_)) type_error(evaluable, log/2)
functions/log_2/tests.lgt lgt_log_2_13 error(evaluation_error(_)) type_error(evaluable, log/2)
functions/log_2/tests.lgt lgt_log_2_14 error(evaluation_error(undefined)) type_error(evaluable, log/2)
functions/log_2/tests.lgt lgt_log_2_15 error(evaluation_error(undefined)) type_error(evaluable, log/2)
functions/pi_0/tests.lgt iso_pi_0_01 true(X=~=3.1415927) fail
functions/power_2/tests.lgt iso_power_2_02 true(X=~= -125.0) fail
functions/power_2/tests.lgt lgt_power_2_14 errors([evaluation_error(undefined), evaluation_error(zero_divisor)]) evaluation_error(float_overflow)
functions/round_1/tests.lgt eclipse_round_1_07 true(X== -3) fail
functions/round_1/tests.lgt eclipse_round_1_08 true(X== -4) fail
functions/round_1/tests.lgt lgt_round_1_13 error(type_error(float, 9)) true
functions/sign_1/tests.lgt sign_1_integer_01 true assertion/1
functions/sign_1/tests.lgt sign_1_integer_02 true assertion/1
functions/sign_1/tests.lgt sign_1_integer_03 true assertion/1
functions/sign_1/tests.lgt sign_1_float_01 true assertion/1
functions/sign_1/tests.lgt sign_1_float_02 true assertion/1
functions/sign_1/tests.lgt sign_1_float_03 true assertion/1
functions/tan_1/tests.lgt iso_tan_1_01 true(X=~=0.5463) fail
functions/truncate_1/tests.lgt lgt_truncate_1_06 error(type_error(float, 9)) true
logical_update_semantics/tests.lgt logical_update_semantics_asserta true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_assertz true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_retract true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_retractall true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_clause_assert true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_clause_retract true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_clause_abolish true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_negation true(L==[2, 4, 6]) existence_error(procedure, :: /2)
predicates/abolish_1/tests.lgt iso_abolish_1_03 errors([type_error(predicate_indicator, foo), type_error(predicate_indicator, user:foo)]) unsupported
predicates/abolish_1/tests.lgt iso_abolish_1_04 errors([type_error(predicate_indicator, foo(X)), type_error(predicate_indicator, user:foo(X))]) unsupported
predicates/abolish_1/tests.lgt iso_abolish_1_05 errors([permission_error(modify, static_procedure, abolish/1), permission_error(modify, static_procedure, user:abolish/1)]) unsupported
predicates/abolish_1/tests.lgt eddbali_abolish_1_07 true(L==[ant, bee]) fail
predicates/abolish_1/tests.lgt eddbali_abolish_1_08 errors([permission_error(modify, static_procedure, bar/1), permission_error(modify, static_procedure, user:bar/1)]) unsupported
predicates/abolish_1/tests.lgt eddbali_abolish_1_10 error(domain_error(not_less_than_zero, -1)) domain_error(not_less_than_zero, -1)
predicates/abolish_1/tests.lgt eddbali_abolish_1_13 errors([type_error(predicate_indicator, insect), type_error(predicate_indicator, user:insect)]) unsupported
predicates/abolish_1/tests.lgt lgt_abolish_1_16 errors([existence_error(procedure, baz/2), existence_error(procedure, user:baz/2)]) unsupported
predicates/arg_3/tests.lgt eddbali_arg_3_13 error(domain_error(not_less_than_zero, -3)) domain_error(not_less_than_zero, -3)
predicates/arg_3/tests.lgt lgt_arg_3_20 true(Arg==(1, 2, 3)) fail
predicates/asserta_1/tests.lgt iso_asserta_1_07 errors([permission_error(modify, static_procedure, atom/1), permission_error(modify, static_procedure, user:atom/1)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_13 errors([permission_error(modify, static_procedure, true/0), permission_error(modify, static_procedure, user:true/0)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_14 errors([permission_error(modify, static_procedure, fail/0), permission_error(modify, static_procedure, user:fail/0)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_15 errors([permission_error(modify, static_procedure, false/0), permission_error(modify, static_procedure, user:false/0)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_16 errors([permission_error(modify, static_procedure, call/1), permission_error(modify, static_procedure, user:call/1)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_17 errors([permission_error(modify, static_procedure, !/0), permission_error(modify, static_procedure, user:!/0)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_18 errors([permission_error(modify, static_procedure, ,/2), permission_error(modify, static_procedure, user:,/2)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_19 errors([permission_error(modify, static_procedure, ;/2), permission_error(modify, static_procedure, user:;/2)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_20 errors([permission_error(modify, static_procedure, -> /2), permission_error(modify, static_procedure, user: -> /2)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_21 errors([permission_error(modify, static_procedure, catch/3), permission_error(modify, static_procedure, user:catch/3)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_22 errors([permission_error(modify, static_procedure, throw/1), permission_error(modify, static_procedure, user:throw/1)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_23 errors([permission_error(modify, static_procedure, :- /2), permission_error(modify, static_procedure, user: :- /2)]) unsupported
predicates/asserta_1/tests.lgt wg17_asserta_1_24 errors([permission_error(modify, static_procedure, :- /1), permission_error(modify, static_procedure, user: :- /1)]) unsupported
predicates/assertz_1/tests.lgt iso_assertz_1_07 errors([permission_error(modify, static_procedure, atom/1), permission_error(modify, static_procedure, user:atom/1)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_13 errors([permission_error(modify, static_procedure, true/0), permission_error(modify, static_procedure, user:true/0)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_14 errors([permission_error(modify, static_procedure, fail/0), permission_error(modify, static_procedure, user:fail/0)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_15 errors([permission_error(modify, static_procedure, false/0), permission_error(modify, static_procedure, user:false/0)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_16 errors([permission_error(modify, static_procedure, call/1), permission_error(modify, static_procedure, user:call/1)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_17 errors([permission_error(modify, static_procedure, !/0), permission_error(modify, static_procedure, user:!/0)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_18 errors([permission_error(modify, static_procedure, ,/2), permission_error(modify, static_procedure, user:,/2)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_19 errors([permission_error(modify, static_procedure, ;/2), permission_error(modify, static_procedure, user:;/2)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_20 errors([permission_error(modify, static_procedure, -> /2), permission_error(modify, static_procedure, user: -> /2)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_21 errors([permission_error(modify, static_procedure, catch/3), permission_error(modify, static_procedure, user:catch/3)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_22 errors([permission_error(modify, static_procedure, throw/1), permission_error(modify, static_procedure, user:throw/1)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_23 errors([permission_error(modify, static_procedure, :- /2), permission_error(modify, static_procedure, user: :- /2)]) unsupported
predicates/assertz_1/tests.lgt wg17_assertz_1_24 errors([permission_error(modify, static_procedure, :- /1), permission_error(modify, static_procedure, user: :- /1)]) unsupported
predicates/atom_chars_2/tests.lgt iso_atom_chars_2_02 true(L==[[, ]]) fail
predicates/atom_length_2/tests.lgt iso_atom_length_2_02 true(N==17) fail
predicates/atom_length_2/tests.lgt eddbali_atom_length_2_08 error(domain_error(not_less_than_zero, -4)) domain_error(not_less_than_zero, -4)
predicates/bagof_3/tests.lgt iso_bagof_3_01 true(S==[1, 2]) load
predicates/bagof_3/tests.lgt iso_bagof_3_02 true(X==[1, 2]) load
predicates/bagof_3/tests.lgt iso_bagof_3_03 true(S==[Y, Z]) load
predicates/bagof_3/tests.lgt iso_bagof_3_04 false load
predicates/bagof_3/tests.lgt iso_bagof_3_05 true((LL==[[1]-1, [1]-2];LL==[[1]-2, [1]-1])) load
predicates/bagof_3/tests.lgt iso_bagof_3_06 variant(L, [f(a, _), f(_, b)]) load
predicates/bagof_3/tests.lgt iso_bagof_3_07 true(S==[1, 2]) load
predicates/bagof_3/tests.lgt iso_bagof_3_08 variant(S, [1, _, 2]) load
predicates/bagof_3/tests.lgt iso_bagof_3_09 true(S==[3]) load
predicates/bagof_3/tests.lgt iso_bagof_3_10 variant(LL, [[A, _]-A, [_]-1]) load
predicates/bagof_3/tests.lgt iso_bagof_3_11 variant(Y, f(_)) load
predicates/bagof_3/tests.lgt iso_bagof_3_12 true(LL==[[1, 1, 2]-1, [1, 2, 2]-2]) load
predicates/bagof_3/tests.lgt iso_bagof_3_13 error(instantiation_error) load
predicates/bagof_3/tests.lgt iso_bagof_3_14 errors([type_error(callable, 1), type_error(callable, user:1)]) load
predicates/bagof_3/tests.lgt iso_bagof_3_15 subsumes([_], LL) load
predicates/bagof_3/tests.lgt iso_bagof_3_16 true(var(X)) load
predicates/bagof_3/tests.lgt iso_bagof_3_17 true load
predicates/bagof_3/tests.lgt eclipse_bagof_3_18 error(type_error(list, 12)) load
predicates/bagof_3/tests.lgt eclipse_bagof_3_19 error(type_error(list, [1|2])) load
predicates/bagof_3/tests.lgt lgt_bagof_3_20 true(L==['B', 'A', 'C']) load
predicates/bagof_3/tests.lgt lgt_bagof_3_21 true(L==['B', 'A', 'C']) load
predicates/bagof_3/tests.lgt lgt_bagof_3_23 errors([existence_error(procedure, foobar/1), existence_error(procedure, user:foobar/1)]) load
predicates/bagof_3/tests.lgt lgt_bagof_3_24 variant(LL, [s(_, [1, 1, 1])]) load
predicates/bagof_3/tests.lgt lgt_bagof_3_25 variant(LL, [s(_, _, [1, 1, 1])]) load
predicates/bagof_3/tests.lgt lgt_bagof_3_26 true load
predicates/bagof_3/tests.lgt lgt_bagof_3_27 true load
predicates/bagof_3/tests.lgt lgt_bagof_3_28 true load
predicates/bagof_3/tests.lgt lgt_bagof_3_29 true load
predicates/bagof_3/tests.lgt lgt_bagof_3_30 ball(err) load
predicates/bagof_3/tests.lgt wg17_bagof_3_31 false load
predicates/bagof_3/tests.lgt wg17_bagof_3_32 true((variant(Ls, [[E, _, _], [_, E, _], [_, _, E]]);variant(Ls, [[_, _, E], [_, E, _], [E, _, _]]))) load
predicates/bagof_3/tests.lgt wg17_bagof_3_33 true((L==[A, B, C];L==[C, B, A])) load
predicates/call_N/tests.lgt lgt_call_N_10 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt lgt_call_N_12 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt lgt_call_N_14 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt lgt_call_N_16 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt lgt_call_N_18 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt lgt_call_N_20 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt lgt_call_N_22 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt lgt_call_N_23 errors([existence_error(procedure, foobar/1), existence_error(procedure, user:foobar/1)]) unsupported
predicates/call_N/tests.lgt eclipse_call_N_25 errors([type_error(callable, (fail, 3)), type_error(callable, 3), type_error(callable, (user:fail, user:3)), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt eclipse_call_N_26 errors([type_error(callable, (!;3)), type_error(callable, 3), type_error(callable, (user:!;user:3)), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt eclipse_call_N_27 errors([type_error(callable, (fail->3)), type_error(callable, 3), type_error(callable, (user:fail->user:3)), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt eclipse_call_N_29 errors([type_error(callable, (fail, 3)), type_error(callable, 3), type_error(callable, (user:fail, user:3)), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt eclipse_call_N_30 errors([type_error(callable, (!;3)), type_error(callable, 3), type_error(callable, (user:!;user:3)), type_error(callable, user:3)]) unsupported
predicates/call_N/tests.lgt eclipse_call_N_31 errors([type_error(callable, (fail->3)), type_error(callable, 3), type_error(callable, (user:fail->user:3)), type_error(callable, user:3)]) unsupported
predicates/char_code_2/tests.lgt iso_char_code_2_01 true(Code==97) load
predicates/char_code_2/tests.lgt iso_char_code_2_02 true(Char==Atom) load
predicates/char_code_2/tests.lgt iso_char_code_2_03 true(Char==c) load
predicates/char_code_2/tests.lgt iso_char_code_2_04 true load
predicates/char_code_2/tests.lgt iso_char_code_2_05 true load
predicates/char_code_2/tests.lgt iso_char_code_2_06 error(type_error(character, ab)) load
predicates/char_code_2/tests.lgt iso_char_code_2_07 error(instantiation_error) load
predicates/char_code_2/tests.lgt eddbali_char_code_2_08 error(type_error(integer, x)) load
predicates/char_code_2/tests.lgt eddbali_char_code_2_09 error(representation_error(character_code)) load
predicates/char_code_2/tests.lgt lgt_char_code_2_10 true load
predicates/char_code_2/tests.lgt lgt_char_code_2_11 error(type_error(character, 42)) load
predicates/char_code_2/tests.lgt lgt_char_code_2_12 all((char_code(Char, Code0), char_code(Char, Code), Code0==Code)) load
predicates/clause_2/tests.lgt iso_clause_2_09 true fail
predicates/clause_2/tests.lgt iso_clause_2_10 errors([permission_error(access, private_procedure, atom/1), permission_error(access, private_procedure, user:atom/1)]) unsupported
predicates/current_input_1/tests.lgt sics_current_input_1_02 error(domain_error(stream, foo)) domain_error(stream_or_alias, foo)
predicates/current_input_1/tests.lgt sics_current_input_1_04 error(domain_error(stream, S)) existence_error(stream, '$stream'(3))
predicates/current_input_1/tests.lgt lgt_current_input_1_06 true existence_error(procedure, ground/1)
predicates/current_op_3/tests.lgt iso_current_op_3_01 exists(P-T==1200-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_02 exists(P-T==1200-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_03 exists(P-T==1200-fx) load
predicates/current_op_3/tests.lgt iso_current_op_3_04 exists(P-T==1200-fx) load
predicates/current_op_3/tests.lgt iso_current_op_3_05 exists(P-T==1100-xfy) load
predicates/current_op_3/tests.lgt iso_current_op_3_06 exists(P-T==1050-xfy) load
predicates/current_op_3/tests.lgt iso_current_op_3_07 exists(P-T==1000-xfy) load
predicates/current_op_3/tests.lgt iso_current_op_3_08 exists(P-T==900-fy) load
predicates/current_op_3/tests.lgt iso_current_op_3_09 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_10 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_11 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_12 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_13 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_14 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_15 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_16 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_17 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_18 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_19 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_20 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_21 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_22 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_23 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_24 exists(P-T==700-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_25 exists(P-T==500-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_26 exists(P-T==500-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_27 exists(P-T==500-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_28 exists(P-T==500-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_29 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_30 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_31 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_32 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_33 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_34 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_35 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_36 exists(P-T==200-xfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_37 exists(P-T==200-xfy) load
predicates/current_op_3/tests.lgt iso_current_op_3_38 exists(P-T==200-fy) load
predicates/current_op_3/tests.lgt iso_current_op_3_39 exists(P-T==200-fy) load
predicates/current_op_3/tests.lgt iso_current_op_3_40 true((Contents==[b, a, z, q, u, u, x];Contents==[q, u, u, x, b, a, z])) load
predicates/current_op_3/tests.lgt sics_current_op_3_41 error(domain_error(operator_priority, 1201)) load
predicates/current_op_3/tests.lgt sics_current_op_3_42 error(domain_error(operator_specifier, yfy)) load
predicates/current_op_3/tests.lgt sics_current_op_3_43 errors([type_error(atom, 0), domain_error(operator_specifier, 0)]) load
predicates/current_op_3/tests.lgt sics_current_op_3_44 error(type_error(atom, 5)) load
predicates/current_op_3/tests.lgt iso_current_op_3_45 true load
predicates/current_op_3/tests.lgt iso_current_op_3_46 exists(P-T==400-yfx) load
predicates/current_op_3/tests.lgt iso_current_op_3_47 exists(P-T==200-fy) load
predicates/current_op_3/tests.lgt lgt_current_op_3_48 true(PTs==[200-fy, 500-yfx]) load
predicates/current_op_3/tests.lgt lgt_current_op_3_49 errors([type_error(integer, a), domain_error(operator_priority, a)]) load
predicates/current_op_3/tests.lgt lgt_current_op_3_50 false load
predicates/current_op_3/tests.lgt lgt_current_op_3_51 false load
predicates/current_op_3/tests.lgt lgt_current_op_3_52 false load
predicates/current_output_1/tests.lgt sics_current_output_1_02 error(domain_error(stream, foo)) domain_error(stream_or_alias, foo)
predicates/current_output_1/tests.lgt sics_current_output_1_04 error(domain_error(stream, S)) existence_error(stream, '$stream'(3))
predicates/current_output_1/tests.lgt sics_current_output_1_06 true existence_error(procedure, ground/1)
predicates/current_prolog_flag_2/tests.lgt iso_current_prolog_flag_2_01 true domain_error(prolog_flag, debug)
predicates/current_prolog_flag_2/tests.lgt iso_current_prolog_flag_2_07 true((Debug==on;Debug==off)) domain_error(prolog_flag, debug)
predicates/current_prolog_flag_2/tests.lgt eddbali_current_prolog_flag_2_12 true domain_error(flag_value, unknown+warning)
predicates/current_prolog_flag_2/tests.lgt eddbali_current_prolog_flag_2_13 false domain_error(flag_value, unknown+warning)
predicates/current_prolog_flag_2/tests.lgt commons_current_prolog_flag_2_16 true(atom(Dialect)) domain_error(prolog_flag, dialect)
predicates/current_prolog_flag_2/tests.lgt commons_current_prolog_flag_2_17 true((integer(Major), integer(Minor), integer(Patch))) domain_error(prolog_flag, version_data)
predicates/current_prolog_flag_2/tests.lgt commons_current_prolog_flag_2_18 true(functor(VersionData, Dialect, _)) domain_error(prolog_flag, dialect)
predicates/current_prolog_flag_2/tests.lgt lgt_current_prolog_flag_2_20 true flag(Flag)
predicates/findall_3/tests.lgt iso_findall_3_08 errors([type_error(callable, 4), type_error(callable, user:4)]) unsupported
predicates/findall_3/tests.lgt lgt_findall_3_13 errors([existence_error(procedure, foo/1), existence_error(procedure, user:foo/1)]) unsupported
predicates/findall_4/tests.lgt commons_findall_4_06 true assertion/2
predicates/findall_4/tests.lgt commons_findall_4_09 errors([type_error(callable, 4), type_error(callable, user:4)]) unsupported
predicates/findall_4/tests.lgt commons_findall_4_12 error(type_error(list, [A|1])) true
predicates/findall_4/tests.lgt commons_findall_4_13 error(type_error(list, 12)) true
predicates/findall_4/tests.lgt commons_findall_4_14 errors([existence_error(procedure, foo/1), existence_error(procedure, user:foo/1)]) unsupported
predicates/functor_3/tests.lgt iso_functor_3_18 error(domain_error(not_less_than_zero, -1)) domain_error(not_less_than_zero, -1)
predicates/nl_1/tests.lgt lgt_nl_1_04 error(permission_error(output, binary_stream, _)) permission_error(output, binary_stream, bin)
predicates/nl_1/tests.lgt lgt_nl_1_05 error(existence_error(stream, Stream)) existence_error(stream, '$stream'(4))
predicates/nl_1/tests.lgt lgt_nl_1_06 error(existence_error(stream, Stream)) existence_error(stream, '$stream'(5))
predicates/nl_1/tests.lgt lgt_nl_0_02 error(permission_error(output, binary_stream, Stream)) permission_error(output, binary_stream, '$stream'(8))
predicates/not_1/tests.lgt lgt_not_1_10 errors([existence_error(procedure, foobar/1), existence_error(procedure, user:foobar/1)]) unsupported
predicates/number_chars_2/tests.lgt wg17_number_chars_2_40 true(X==32) syntax_error('0\' ')
predicates/number_chars_2/tests.lgt lgt_number_chars_2_43 error(syntax_error(_)) fail
predicates/number_codes_2/tests.lgt wg17_number_codes_2_43 true(X==32) syntax_error('0\' ')
predicates/once_1/tests.lgt eddbali_once_1_06 errors([type_error(callable, 3), type_error(callable, user:3)]) unsupported
predicates/once_1/tests.lgt lgt_once_1_08 errors([existence_error(procedure, foobar/1), existence_error(procedure, user:foobar/1)]) unsupported
predicates/op_3/tests.lgt iso_op_3_04 error(domain_error(operator_priority, -30)) domain_error(operator_priority, -30)
predicates/op_3/tests.lgt iso_op_3_20 true(Assertion) fail
predicates/op_3/tests.lgt lgt_op_3_34 true assertion/1
predicates/open_4/tests.lgt iso_open_4_01 true fail
predicates/open_4/tests.lgt iso_open_4_03 true fail
predicates/open_4/tests.lgt sics_open_4_15 error(existence_error(source_sink, Path)) existence_error(source_sink, '/Users/anchu/projects/GitHub/mosa-prolog/tests/tmp/suite/predicates/open_4/nonexistent')
predicates/open_4/tests.lgt sics_open_4_16 error(permission_error(open, source_sink, alias(a))) permission_error(open, source_sink, alias(a))
predicates/open_4/tests.lgt lgt_open_4_20 error(domain_error(stream_option, alias(1))) type_error(atom, 1)
predicates/open_4/tests.lgt lgt_open_4_22 error(domain_error(stream_option, eof_action(1))) type_error(atom, 1)
predicates/open_4/tests.lgt lgt_open_4_24 error(domain_error(stream_option, reposition(1))) type_error(atom, 1)
predicates/open_4/tests.lgt lgt_open_4_26 error(domain_error(stream_option, type(1))) type_error(atom, 1)
predicates/open_4/tests.lgt lgt_open_4_31 errors([permission_error(open, source_sink, _), existence_error(source_sink, _)]) unsupported
predicates/open_4/tests.lgt lgt_open_4_41 true(Action==eof_code) existence_error(procedure, :: /2)
predicates/open_4/tests.lgt lgt_open_4_42 true(Action==reset) existence_error(procedure, :: /2)
predicates/open_4/tests.lgt lgt_open_4_43 true(Action==error) existence_error(procedure, :: /2)
predicates/print_1/tests.lgt lgt_print_1_02 true(Assertion) fail
predicates/print_1/tests.lgt lgt_print_1_03 true(Assertion) fail
predicates/print_1/tests.lgt lgt_print_1_04 true(Assertion) fail
predicates/print_1/tests.lgt lgt_print_1_05 true(Assertion) fail
predicates/print_2/tests.lgt lgt_print_2_02 true(Assertion) existence_error(stream, out)
predicates/print_2/tests.lgt lgt_print_2_03 true(Assertion) existence_error(stream, out)
predicates/print_2/tests.lgt lgt_print_2_04 true(Assertion) existence_error(stream, out)
predicates/print_2/tests.lgt lgt_print_2_05 true(Assertion) existence_error(stream, out)
predicates/print_2/tests.lgt lgt_print_2_08 error(existence_error(stream, S)) existence_error(stream, '$stream'(8))
predicates/print_2/tests.lgt lgt_print_2_09 error(existence_error(stream, S)) existence_error(stream, '$stream'(9))
predicates/print_2/tests.lgt lgt_print_2_11 error(permission_error(output, binary_stream, S)) permission_error(output, binary_stream, '$stream'(10))
predicates/print_2/tests.lgt lgt_print_2_12 error(permission_error(output, stream, s)) permission_error(output, stream, s)
predicates/print_2/tests.lgt lgt_print_2_13 error(permission_error(output, binary_stream, _)) existence_error(stream, s)
predicates/put_byte_2/tests.lgt iso_put_byte_2_03 error(instantiation_error) instantiation_error
predicates/put_byte_2/tests.lgt iso_put_byte_2_04 error(type_error(byte, ty)) type_error(byte, ty)
predicates/put_byte_2/tests.lgt sics_put_byte_2_06 error(instantiation_error) instantiation_error
predicates/put_byte_2/tests.lgt sics_put_byte_2_07 error(existence_error(stream, S)) existence_error(stream, '$stream'(8))
predicates/put_byte_2/tests.lgt sics_put_byte_2_08 error(existence_error(stream, S)) existence_error(stream, '$stream'(9))
predicates/put_byte_2/tests.lgt sics_put_byte_2_09 error(permission_error(output, stream, S)) permission_error(output, stream, '$stream'(10))
predicates/put_byte_2/tests.lgt sics_put_byte_2_10 error(permission_error(output, text_stream, S)) true
predicates/put_byte_2/tests.lgt sics_put_byte_2_11 error(type_error(byte, -1)) type_error(byte, -1)
predicates/put_byte_2/tests.lgt lgt_put_byte_2_14 error(permission_error(output, stream, s)) permission_error(output, stream, s)
predicates/put_byte_2/tests.lgt lgt_put_byte_2_15 error(permission_error(output, text_stream, _)) existence_error(stream, s)
predicates/put_byte_2/tests.lgt lgt_put_byte_2_16 error(type_error(byte, 256)) type_error(byte, 256)
predicates/put_byte_2/tests.lgt lgt_put_byte_2_17 error(type_error(byte, 256)) existence_error(stream, st_o)
predicates/put_byte_2/tests.lgt lgt_put_byte_2_19 true(Assertion) existence_error(stream, st_o)
predicates/put_byte_2/tests.lgt lgt_put_byte_2_21 true(Assertion) existence_error(stream, st_o)
predicates/put_char_2/tests.lgt iso_put_char_2_04 true(Assertion) existence_error(stream, st_o)
predicates/put_char_2/tests.lgt iso_put_char_2_05 error(instantiation_error) instantiation_error
predicates/put_char_2/tests.lgt iso_put_char_2_06 error(type_error(character, ty)) existence_error(stream, st_o)
predicates/put_char_2/tests.lgt sics_put_char_2_09 error(existence_error(stream, S)) existence_error(stream, '$stream'(9))
predicates/put_char_2/tests.lgt sics_put_char_2_10 error(existence_error(stream, S)) existence_error(stream, '$stream'(10))
predicates/put_char_2/tests.lgt sics_put_char_2_12 error(permission_error(output, binary_stream, S)) permission_error(output, binary_stream, '$stream'(11))
predicates/put_char_2/tests.lgt lgt_put_char_2_13 error(permission_error(output, stream, s)) permission_error(output, stream, s)
predicates/put_char_2/tests.lgt lgt_put_char_2_14 error(permission_error(output, binary_stream, _)) existence_error(stream, s)
predicates/put_char_2/tests.lgt lgt_put_char_2_15 error(type_error(character, 1)) type_error(character, 1)
predicates/put_char_2/tests.lgt lgt_put_char_2_16 error(type_error(character, 1)) existence_error(stream, st_o)
predicates/put_code_2/tests.lgt iso_put_code_2_03 error(instantiation_error) instantiation_error
predicates/put_code_2/tests.lgt iso_put_code_2_04 error(type_error(integer, ty)) existence_error(stream, st_o)
predicates/put_code_2/tests.lgt iso_put_code_2_07 error(existence_error(stream, S)) existence_error(stream, '$stream'(7))
predicates/put_code_2/tests.lgt iso_put_code_2_08 error(existence_error(stream, S)) existence_error(stream, '$stream'(8))
predicates/put_code_2/tests.lgt iso_put_code_2_10 error(permission_error(output, binary_stream, S)) permission_error(output, binary_stream, '$stream'(9))
predicates/put_code_2/tests.lgt lgt_put_code_2_13 error(permission_error(output, stream, s)) permission_error(output, stream, s)
predicates/put_code_2/tests.lgt lgt_put_code_2_14 error(permission_error(output, binary_stream, _)) existence_error(stream, s)
predicates/put_code_2/tests.lgt lgt_put_code_2_15 error(type_error(integer, 65.0)) type_error(integer, 65.0)
predicates/put_code_2/tests.lgt lgt_put_code_2_16 error(type_error(integer, 65.0)) existence_error(stream, st_o)
predicates/read_term_3/tests.lgt iso_read_term_3_01 true(Assertion) fail
predicates/read_term_3/tests.lgt iso_read_term_3_02 true(Assertion) fail
predicates/read_term_3/tests.lgt iso_read_term_3_03 true existence_error(stream, in)
predicates/read_term_3/tests.lgt iso_read_term_3_04 true(Assertion) fail
predicates/read_term_3/tests.lgt iso_read_term_3_05 true(Assertion) fail
predicates/read_term_3/tests.lgt sics_read_term_3_10 error(instantiation_error) instantiation_error
predicates/read_term_3/tests.lgt sics_read_term_3_14 error(domain_error(read_option, bar)) domain_error(read_option, bar)
predicates/read_term_3/tests.lgt sics_read_term_3_17 error(existence_error(stream, S)) existence_error(stream, '$stream'(13))
predicates/read_term_3/tests.lgt sics_read_term_3_18 error(existence_error(stream, S)) existence_error(stream, '$stream'(14))
predicates/read_term_3/tests.lgt sics_read_term_3_19 error(permission_error(input, binary_stream, S)) permission_error(input, binary_stream, '$stream'(15))
predicates/read_term_3/tests.lgt sics_read_term_3_20 error(permission_error(input, binary_stream, S)) permission_error(input, binary_stream, '$stream'(16))
predicates/read_term_3/tests.lgt sics_read_term_3_21 true(Value==past) existence_error(stream, in)
predicates/read_term_3/tests.lgt sics_read_term_3_23 true(X==Integer) fail
predicates/read_term_3/tests.lgt sics_read_term_3_24 true(X==Integer) fail
predicates/read_term_3/tests.lgt lgt_read_term_3_25 true(Term==end_of_file) existence_error(stream, in)
predicates/read_term_3/tests.lgt lgt_read_term_3_26 error(permission_error(input, stream, s)) permission_error(input, stream, s)
predicates/read_term_3/tests.lgt lgt_read_term_3_27 error(permission_error(input, binary_stream, _)) existence_error(stream, s)
predicates/read_term_3/tests.lgt lgt_read_term_3_28 true assertion
predicates/read_term_3/tests.lgt lgt_read_term_3_30 true existence_error(stream, in)
predicates/read_term_3/tests.lgt lgt_read_term_3_31 true existence_error(stream, in)
predicates/read_term_3/tests.lgt lgt_read_term_3_32 error(domain_error(read_option, variables(a))) fail
predicates/read_term_3/tests.lgt lgt_read_term_3_33 error(domain_error(read_option, variables([_|a]))) fail
predicates/read_term_3/tests.lgt lgt_read_term_3_34 error(domain_error(read_option, variable_names(a))) fail
predicates/read_term_3/tests.lgt lgt_read_term_3_35 error(domain_error(read_option, variable_names([_|a]))) fail
predicates/read_term_3/tests.lgt lgt_read_term_3_36 error(domain_error(read_option, singletons(a))) fail
predicates/read_term_3/tests.lgt lgt_read_term_3_37 error(domain_error(read_option, singletons([_|a]))) fail
predicates/read_term_3/tests.lgt lgt_read_term_3_38 error(syntax_error(_)) syntax_error('行 1 列 3: 項が閉じていません 終端')
predicates/read_term_3/tests.lgt lgt_read_term_3_39 error(syntax_error(_)) syntax_error('行 1 列 5: 項が閉じていません 終端')
predicates/retract_1/tests.lgt iso_retract_1_11 errors([permission_error(modify, static_procedure, atom/1), permission_error(modify, static_procedure, user:atom/1)]) unsupported
predicates/retract_1/tests.lgt lgt_retract_1_13 errors([permission_error(modify, static_procedure, elk/1), permission_error(modify, static_procedure, user:elk/1)]) unsupported
predicates/retract_1/tests.lgt eclipse_retract_1_15 errors([permission_error(modify, static_procedure, retract/1), permission_error(modify, static_procedure, user:retract/1)]) unsupported
predicates/retract_1/tests.lgt eclipse_retract_1_16 errors([permission_error(modify, static_procedure, true/0), permission_error(modify, static_procedure, user:true/0)]) unsupported
predicates/retract_1/tests.lgt eclipse_retract_1_17 errors([permission_error(modify, static_procedure, catch/3), permission_error(modify, static_procedure, user:catch/3)]) unsupported
predicates/retract_1/tests.lgt eclipse_retract_1_18 errors([permission_error(modify, static_procedure, repeat/0), permission_error(modify, static_procedure, user:repeat/0)]) unsupported
predicates/retractall_1/tests.lgt iso_retractall_1_01 true({current_predicate(foo/1)}) existence_error(procedure, {}/1)
predicates/retractall_1/tests.lgt iso_retractall_1_06 errors([permission_error(modify, static_procedure, retractall/1), permission_error(modify, static_procedure, user:retractall/1)]) unsupported
predicates/retractall_1/tests.lgt eclipse_retractall_1_12 errors([permission_error(modify, static_procedure, elk/1), permission_error(modify, static_procedure, user:elk/1)]) unsupported
predicates/retractall_1/tests.lgt lgt_retractall_1_13 errors([permission_error(modify, static_procedure, true/0), permission_error(modify, static_procedure, user:true/0)]) unsupported
predicates/retractall_1/tests.lgt lgt_retractall_1_14 errors([permission_error(modify, static_procedure, catch/3), permission_error(modify, static_procedure, user:catch/3)]) unsupported
predicates/retractall_1/tests.lgt lgt_retractall_1_15 errors([permission_error(modify, static_procedure, repeat/0), permission_error(modify, static_procedure, user:repeat/0)]) unsupported
predicates/set_prolog_flag_2/tests.lgt iso_set_prolog_flag_2_05 error(domain_error(flag_value, debug+trace)) domain_error(prolog_flag, debug)
predicates/set_prolog_flag_2/tests.lgt eddbali_set_prolog_flag_2_06 error(permission_error(modify, flag, max_arity)) domain_error(flag_value, max_arity+40)
predicates/set_prolog_flag_2/tests.lgt lgt_set_prolog_flag_2_13 true flag(Flag)
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_01 error(instantiation_error) instantiation_error
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_02 error(instantiation_error) instantiation_error
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_03 errors([domain_error(stream_or_alias, foo), existence_error(stream, foo)]) domain_error(stream_or_alias, foo)
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_04 error(existence_error(stream, Stream)) existence_error(stream, '$stream'(4))
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_05 error(domain_error(stream_position, foo)) existence_error(stream, st_i)
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_06 error(permission_error(reposition, stream, Stream)) existence_error(source_sink, '/Users/anchu/projects/GitHub/mosa-prolog/tests/tmp/suite/predicates/set_stream_position_2/terms.pl')
predicates/set_stream_position_2/tests.lgt lgt_set_stream_position_2_07 true(Term1==Term3) existence_error(source_sink, '/Users/anchu/projects/GitHub/mosa-prolog/tests/tmp/suite/predicates/set_stream_position_2/terms.pl')
predicates/set_stream_position_2/tests.lgt lgt_set_stream_position_2_08 true(Byte1==Byte3) existence_error(source_sink, '/Users/anchu/projects/GitHub/mosa-prolog/tests/tmp/suite/predicates/set_stream_position_2/bytes.pl')
predicates/setof_3/tests.lgt iso_setof_3_01 true(S==[1, 2]) load
predicates/setof_3/tests.lgt iso_setof_3_02 true(X==[1, 2]) load
predicates/setof_3/tests.lgt iso_setof_3_03 true(S==[1, 2]) load
predicates/setof_3/tests.lgt iso_setof_3_04 true(S==[2]) load
predicates/setof_3/tests.lgt iso_setof_3_05 true((S==[Z, Y];S==[Y, Z])) load
predicates/setof_3/tests.lgt iso_setof_3_06 false load
predicates/setof_3/tests.lgt iso_setof_3_07 true(LL==[[1]-1, [1]-2]) load
predicates/setof_3/tests.lgt iso_setof_3_08 variant(L, [f(_, b), f(a, _)]) load
predicates/setof_3/tests.lgt iso_setof_3_09 true(S==[1, 2]) load
predicates/setof_3/tests.lgt iso_setof_3_10 variant(S, [_, 1, 2]) load
predicates/setof_3/tests.lgt iso_setof_3_11 true(S==[3]) load
predicates/setof_3/tests.lgt iso_setof_3_12 true load
predicates/setof_3/tests.lgt iso_setof_3_13 variant(Y, f(_)) load
predicates/setof_3/tests.lgt iso_setof_3_14 true load
predicates/setof_3/tests.lgt iso_setof_3_15 true load
predicates/setof_3/tests.lgt iso_setof_3_16 true((U==a, V==a)) load
predicates/setof_3/tests.lgt iso_setof_3_17 true load
predicates/setof_3/tests.lgt iso_setof_3_18 true load
predicates/setof_3/tests.lgt iso_setof_3_19 false load
predicates/setof_3/tests.lgt iso_setof_3_20 true load
predicates/setof_3/tests.lgt iso_setof_3_21 true(LL==[[1, 2]-1, [1, 2]-2]) load
predicates/setof_3/tests.lgt iso_setof_3_22 true(L==[1-[1, 2], 2-[1, 2]]) load
predicates/setof_3/tests.lgt iso_setof_3_23 true load
predicates/setof_3/tests.lgt iso_setof_3_24 true load
predicates/setof_3/tests.lgt eddbali_setof_3_26 errors([type_error(callable, (true;4)), type_error(callable, user:(true;4)), type_error(callable, 4), type_error(callable, user:4)]) load
predicates/setof_3/tests.lgt sics_setof_3_27 errors([type_error(callable, 1), type_error(callable, user:1)]) load
predicates/setof_3/tests.lgt sics_setof_3_28 true(A==[]) load
predicates/setof_3/tests.lgt sics_setof_3_29 error(type_error(list, [A|1])) load
predicates/setof_3/tests.lgt eclipse_setof_3_30 error(type_error(list, 12)) load
predicates/setof_3/tests.lgt eclipse_setof_3_31 error(type_error(list, [1|2])) load
predicates/setof_3/tests.lgt lgt_setof_3_32 true(L==['A', 'B', 'C']) load
predicates/setof_3/tests.lgt lgt_setof_3_33 true(L==['A', 'B', 'C']) load
predicates/setof_3/tests.lgt lgt_setof_3_34 true load
predicates/setof_3/tests.lgt lgt_setof_3_35 error(instantiation_error) load
predicates/setof_3/tests.lgt lgt_setof_3_37 errors([existence_error(procedure, foobar/1), existence_error(procedure, user:foobar/1)]) load
predicates/setof_3/tests.lgt lgt_setof_3_38 variant(LL, [s(_, [1])]) load
predicates/setof_3/tests.lgt lgt_setof_3_39 variant(LL, [s(_, _, [1])]) load
predicates/setof_3/tests.lgt lgt_setof_3_40 true load
predicates/setof_3/tests.lgt lgt_setof_3_41 true load
predicates/setof_3/tests.lgt lgt_setof_3_42 true load
predicates/setof_3/tests.lgt lgt_setof_3_43 true load
predicates/setof_3/tests.lgt lgt_bagof_3_44 ball(err) load
predicates/setof_3/tests.lgt wg17_setof_3_45 false load
predicates/setof_3/tests.lgt wg17_setof_3_46 true((variant(Ls, [[E, _, _], [_, E, _], [_, _, E]]);variant(Ls, [[_, _, E], [_, E, _], [E, _, _]]))) load
predicates/setof_3/tests.lgt wg17_setof_3_47 true((L==[A, B, C];L==[C, B, A])) load
predicates/stream_property_2/tests.lgt iso_stream_property_2_01 true((memberchk(S1-FooPath, L), memberchk(S2-BarPath, L))) fail
predicates/stream_property_2/tests.lgt iso_stream_property_2_02 true((memberchk(FOut, L), memberchk(COut, L))) domain_error(stream_property, output)
predicates/stream_property_2/tests.lgt sics_stream_property_2_03 error(domain_error(stream, foo)) domain_error(stream_or_alias, foo)
predicates/stream_property_2/tests.lgt sics_stream_property_2_05a deterministic fail
predicates/stream_property_2/tests.lgt sics_stream_property_2_05c deterministic(Action==reset) fail
predicates/stream_property_2/tests.lgt sics_stream_property_2_06a deterministic fail
predicates/stream_property_2/tests.lgt sics_stream_property_2_06d deterministic(Mode==append) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_08a true fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_08b true(Action==reset) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_09a true fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_09c true(Mode==append) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_10a true fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_10c true(Mode==append) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_11 error(existence_error(stream, _)) existence_error(stream, '$stream'(4))
predicates/stream_property_2/tests.lgt lgt_stream_property_2_12 error(existence_error(stream, _)) existence_error(stream, '$stream'(5))
predicates/stream_property_2/tests.lgt lgt_stream_property_2_16 true(File==Path) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_19 true(Mode==read) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_20 false domain_error(stream_property, input)
predicates/stream_property_2/tests.lgt lgt_stream_property_2_21 false permission_error(open, source_sink, alias(out))
predicates/stream_property_2/tests.lgt lgt_stream_property_2_22 true fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_23 true permission_error(open, source_sink, alias(out))
predicates/stream_property_2/tests.lgt lgt_stream_property_2_24 true permission_error(open, source_sink, alias(out))
predicates/stream_property_2/tests.lgt lgt_stream_property_2_28 true(Alias==r) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_29 true(ground(Position)) permission_error(open, source_sink, alias(out))
predicates/stream_property_2/tests.lgt lgt_stream_property_2_30 true(ground(Position)) permission_error(open, source_sink, alias(out))
predicates/stream_property_2/tests.lgt lgt_stream_property_2_31 true(ground(Position)) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_32 true((EndOfStream==not;EndOfStream==at)) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_33 true((EndOfStream==not;EndOfStream==at)) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_34 true(EOFAction==error) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_35 true(EOFAction==eof_code) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_36 true(EOFAction==reset) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_37 true((EOFAction==error;EOFAction==eof_code;EOFAction==reset)) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_38 true(EOFAction==error) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_39 true(EOFAction==eof_code) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_40 true(EOFAction==reset) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_41 true((EOFAction==error;EOFAction==eof_code;EOFAction==reset)) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_46 true(Reposition==true) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_47 true((Reposition==false;Reposition==true)) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_52 true(Type==text) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_53 true(Type==binary) fail
predicates/sub_atom_5/tests.lgt sics_sub_atom_5_14 error(domain_error(not_less_than_zero, -2)) domain_error(not_less_than_zero, -2)
predicates/sub_atom_5/tests.lgt sics_sub_atom_5_15 error(domain_error(not_less_than_zero, -3)) domain_error(not_less_than_zero, -3)
predicates/sub_atom_5/tests.lgt sics_sub_atom_5_16 error(domain_error(not_less_than_zero, -4)) domain_error(not_less_than_zero, -4)
predicates/sub_atom_5/tests.lgt lgt_sub_atom_5_34 true existence_error(procedure, forall/2)
predicates/term_variables_2/tests.lgt iso_term_variables_2_06 true assertion/1
predicates/term_variables_2/tests.lgt iso_term_variables_2_07 true assertion/1
predicates/term_variables_2/tests.lgt iso_term_variables_2_08 true assertion/1
predicates/univ_2/tests.lgt iso_univ_2_13 error(type_error(atom, a(b))) type_error(atomic, a(b))
predicates/write_term_3/tests.lgt iso_write_term_3_001 true(Assertion) fail
predicates/write_term_3/tests.lgt iso_write_term_3_002 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt iso_write_term_3_003 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt iso_write_term_3_004 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt iso_write_term_3_005 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt iso_write_term_3_006 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt iso_write_term_3_007 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt sics_write_term_3_019 error(existence_error(stream, S)) existence_error(stream, '$stream'(10))
predicates/write_term_3/tests.lgt sics_write_term_3_020 error(existence_error(stream, S)) existence_error(stream, '$stream'(11))
predicates/write_term_3/tests.lgt sics_write_term_3_022 error(permission_error(output, binary_stream, S)) permission_error(output, binary_stream, '$stream'(12))
predicates/write_term_3/tests.lgt lgt_write_term_3_023 error(permission_error(output, stream, s)) permission_error(output, stream, s)
predicates/write_term_3/tests.lgt lgt_write_term_3_024 error(permission_error(output, binary_stream, _)) existence_error(stream, s)
predicates/write_term_3/tests.lgt lgt_write_term_3_025 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_026 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_027 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_028 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_029 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_030 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_031 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_032 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_033 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_034 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_035 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_036 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_037 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_038 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_039 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_040 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_041 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_042 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_043 subsumes(['_', _|_], Chars) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_044 subsumes(['_', _|_], Chars) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_045 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_046 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_047 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_048 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_049 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_050 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_051 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_052 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_053 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_054 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_055 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_056 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_057 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_058 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_059 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_060 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_061 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_062 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_063 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_064 true(Assertion) existence_error(stream, out)
predicates/write_term_3/tests.lgt lgt_write_term_3_065 subsumes(['_', _|_], Chars) text_output_contents/1
predicates/write_term_3/tests.lgt lgt_write_term_3_066 subsumes(['_', _|_], Chars) text_output_contents/1
predicates/write_term_3/tests.lgt lgt_write_term_3_067 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_068 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_069 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_071 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_072 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_074 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_075 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_077 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_078 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_080 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_081 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_082 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_083 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_084 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_086 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_088 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_089 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_090 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_091 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_092 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_093 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_095 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_096 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_097 true(Assertion) fail
predicates/write_term_3/tests.lgt sics_write_term_3_103 error(instantiation_error) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_104 error(domain_error(write_option, quoted(fail))) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_105 error(instantiation_error) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_106 error(domain_error(write_option, ignore_ops(fail))) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_107 error(instantiation_error) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_108 error(domain_error(write_option, numbervars(fail))) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_109 error(instantiation_error) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_110 error(domain_error(write_option, variable_names(a))) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_111 error(instantiation_error) suppress_text_output/0
predicates/write_term_3/tests.lgt sics_write_term_3_112 error(instantiation_error) suppress_text_output/0
predicates/write_term_3/tests.lgt lgt_write_term_3_113 error(domain_error(write_option, variable_names(['A'=_|a]))) suppress_text_output/0
predicates/write_term_3/tests.lgt lgt_write_term_3_114 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_115 true(Assertion) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_117 subsumes(['1', ., '0'|_], Contents) text_output_contents/1
predicates/write_term_3/tests.lgt lgt_write_term_3_118 true(Assertion) domain_error(write_option, quoted(false))
predicates/write_term_3/tests.lgt lgt_write_term_3_119 true(Assertion) domain_error(write_option, quoted(true))
predicates/write_term_3/tests.lgt lgt_write_term_3_120 true(Assertion) domain_error(write_option, numbervars(false))
predicates/write_term_3/tests.lgt lgt_write_term_3_121 true(Assertion) domain_error(write_option, numbervars(true))
predicates/write_term_3/tests.lgt lgt_write_term_3_122 true(Assertion) domain_error(write_option, variable_names(['Used'=_]))
syntax/atoms/tests.lgt lgt_atom_09 error(syntax_error(_)) syntax_error('行 1 列 3: 不正なエスケープ \\')
syntax/atoms/tests.lgt lgt_atom_10 error(syntax_error(_)) syntax_error('行 1 列 3: 不正なエスケープ \\')
syntax/atoms/tests.lgt lgt_atom_11 error(syntax_error(_)) syntax_error('行 1 列 3: 不正なエスケープ \\')
syntax/atoms/tests.lgt lgt_atom_12 error(syntax_error(_)) syntax_error('行 1 列 3: 不正なエスケープ \\')
syntax/atoms/tests.lgt lgt_atom_13 error(syntax_error(_)) fail
syntax/atoms/tests.lgt lgt_atom_14 error(syntax_error(_)) fail
syntax/atoms/tests.lgt lgt_atom_17 true(T=='enchanted evening') fail
syntax/comments/tests.lgt iso_block_comment_04 error(syntax_error(_)) syntax_error('行 2 列 1: 項が閉じていません 終端')
syntax/comments/tests.lgt iso_block_comment_05 error(syntax_error(_)) syntax_error('行 1 列 1: コメントが閉じていない /*')
syntax/compounds/tests.lgt lgt_compound_01 true(functor(T, [], 2)) syntax_error('行 1 列 3: 予期しないトークン (')
syntax/compounds/tests.lgt lgt_compound_02 true(functor(T, {}, 2)) syntax_error('行 1 列 3: 予期しないトークン (')
syntax/curly_terms/tests.lgt lgt_curly_bracketed_term_05 true(T1==T2) syntax_error('行 1 列 8: 予期しないトークン (')
syntax/curly_terms/tests.lgt lgt_curly_bracketed_term_06 true(T1==T2) syntax_error('行 1 列 10: 予期しないトークン (')
syntax/curly_terms/tests.lgt lgt_curly_bracketed_term_07 true(Assertion) fail
syntax/curly_terms/tests.lgt lgt_curly_bracketed_term_08 true(Assertion) fail
syntax/curly_terms/tests.lgt lgt_curly_bracketed_term_09 true(Assertion) fail
syntax/curly_terms/tests.lgt lgt_curly_bracketed_term_10 true(Assertion) fail
syntax/curly_terms/tests.lgt lgt_curly_bracketed_term_11 true(Assertion) fail
syntax/directives/discontiguous_1/tests.lgt discontiguous_1_01 true(L==[a, a, a]) load
syntax/directives/discontiguous_1/tests.lgt discontiguous_1_02 true(L==[1, 2]) load
syntax/directives/discontiguous_1/tests.lgt discontiguous_1_03 true(L==[1-2, 3-4]) load
syntax/directives/discontiguous_1/tests.lgt discontiguous_1_04 true(L==[1-2-3, 4-5-6]) load
syntax/directives/discontiguous_1/tests.lgt discontiguous_1_05 true(L==[1-2-3-4, 5-6-7-8]) load
syntax/double_quoted_terms/tests.lgt iso_double_quoted_term_01 true fail
syntax/double_quoted_terms/tests.lgt iso_double_quoted_term_02 true fail
syntax/double_quoted_terms/tests.lgt iso_double_quoted_term_03 true fail
syntax/double_quoted_terms/tests.lgt iso_double_quoted_term_04 true fail
syntax/double_quoted_terms/tests.lgt iso_double_quoted_term_05 true fail
syntax/double_quoted_terms/tests.lgt iso_double_quoted_term_06 true fail
syntax/double_quoted_terms/tests.lgt lgt_double_quoted_term_13 error(syntax_error(_)) fail
syntax/double_quoted_terms/tests.lgt lgt_double_quoted_term_14 error(syntax_error(_)) fail
syntax/lists/tests.lgt lgt_list_06 error(syntax_error(_)) fail
syntax/lists/tests.lgt lgt_list_07 error(syntax_error(_)) syntax_error('行 1 列 4: 予期しないトークン ]')
syntax/lists/tests.lgt lgt_list_08 error(syntax_error(_)) syntax_error('行 1 列 3: 予期しないトークン 終端')
syntax/lists/tests.lgt lgt_list_09 error(syntax_error(_)) syntax_error('行 1 列 8: 予期しないトークン ]')
syntax/lists/tests.lgt lgt_list_10 error(syntax_error(_)) syntax_error('行 1 列 5: 予期しないトークン |')
syntax/numbers/tests.lgt lgt_number_26 true(T==27) syntax_error('行 1 列 3: 不正なエスケープ \\')
syntax/numbers/tests.lgt lgt_number_27 true(T==127) syntax_error('行 1 列 3: 不正なエスケープ \\')
syntax/numbers/tests.lgt lgt_number_28 true(T==32) syntax_error('行 1 列 3: 不正なエスケープ \\')
syntax/numbers/tests.lgt lgt_number_29 true(T==39) syntax_error('行 1 列 4: 引用が閉じていない \'')
syntax/numbers/tests.lgt lgt_number_31 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン 終端')
syntax/numbers/tests.lgt lgt_number_32 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン 終端')
syntax/numbers/tests.lgt lgt_number_33 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン e')
syntax/numbers/tests.lgt lgt_number_34 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン e33')
syntax/numbers/tests.lgt lgt_number_35 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン E33')
syntax/numbers/tests.lgt lgt_number_36 error(syntax_error(_)) syntax_error('行 1 列 7: 予期しないトークン 終端')
syntax/numbers/tests.lgt lgt_number_37 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン B101010')
syntax/numbers/tests.lgt lgt_number_38 error(syntax_error(_)) syntax_error('行 1 列 6: 予期しないトークン 終端')
syntax/numbers/tests.lgt lgt_number_39 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン O31754')
syntax/numbers/tests.lgt lgt_number_40 error(syntax_error(_)) syntax_error('行 1 列 6: 予期しないトークン G84')
syntax/numbers/tests.lgt lgt_number_41 error(syntax_error(_)) syntax_error('行 1 列 2: 予期しないトークン X31AF84')
syntax/operators/tests.lgt iso_operators_01 error(syntax_error(_)) syntax_error('行 1 列 7: 予期しないトークン 終端')
syntax/operators/tests.lgt iso_operators_03 error(syntax_error(_)) syntax_error('行 1 列 6: 予期しないトークン xf')
syntax/operators/tests.lgt iso_operators_05 error(syntax_error(_)) syntax_error('行 1 列 9: 予期しないトークン xfx')
syntax/operators/tests.lgt lgt_operators_40 true op(600, xfy, :)
syntax/terms/tests.lgt iso_term_03 error(syntax_error(_)) fail
syntax/terms/tests.lgt iso_term_04 error(syntax_error(_)) fail
syntax/terms/tests.lgt iso_term_05 error(syntax_error(_)) fail
syntax/terms/tests.lgt wg17_term_09 error(syntax_error(_)) syntax_error('行 1 列 4: 予期しないトークン :-')
syntax/terms/tests.lgt wg17_term_10 error(syntax_error(_)) syntax_error('行 1 列 5: 予期しないトークン b')
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_01a true(L==['Γ', ε, ι, ά, ' ', σ, ο, υ, ' ', κ, ό, σ, μ, ε, !]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_01b true(A=='Γειά σου κόσμε!') load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_02a true(L==[¡, 'H', o, l, a, ' ', m, u, n, d, o, !]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_02b true(A=='¡Hola mundo!') load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_03a true(L==[こ, ん, に, ち, は, 世, 界, !]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_03b true(A=='こんにちは世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_04a true(L==[여, 보, 세, 요, ' ', 세, 계, !]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_04b true(A=='여보세요 세계!') load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_05a true(L==['O', l, á, ' ', m, u, n, d, o, !]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_05b true(A=='Olá mundo!') load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_06a true(L==['З', д, р, а, в, с, т, в, у, л, т, е, !, ' ', м, и, р, !]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_06b true(A=='Здравствулте! мир!') load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_07a true(L==[你, 好, 世, 界, !]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_07b true(A=='你好世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_08a true(L==[🀙, 🀚, 🀛, 🀜, 🀝, 🀞, 🀟, 🀠]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_08b true(A== 🀙🀚🀛🀜🀝🀞🀟🀠) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_09a true(L==['Á', 'À', 'Ã', 'Â', 'Ä', á, à, ã, â, ä]) load
unicode/builtins/tests.lgt lgt_unicode_atom_chars_2_09b true(A=='ÁÀÃÂÄáàãâä') load
unicode/builtins/tests.lgt sics_unicode_atom_chars_2_14 true(L==['P', é, c, s]) load
unicode/builtins/tests.lgt sics_unicode_atom_chars_2_15 true(A=='Pécs') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_01a true(L==[915, 949, 953, 940, 32, 963, 959, 965, 32, 954, 972, 963, 956, 949, 33]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_01b true(A=='Γειά σου κόσμε!') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_02a true(L==[161, 72, 111, 108, 97, 32, 109, 117, 110, 100, 111, 33]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_02b true(A=='¡Hola mundo!') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_03a true(L==[12371, 12435, 12395, 12385, 12399, 19990, 30028, 33]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_03b true(A=='こんにちは世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_04a true(L==[50668, 48372, 49464, 50836, 32, 49464, 44228, 33]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_04b true(A=='여보세요 세계!') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_05a true(L==[79, 108, 225, 32, 109, 117, 110, 100, 111, 33]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_05b true(A=='Olá mundo!') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_06a true(L==[1047, 1076, 1088, 1072, 1074, 1089, 1090, 1074, 1091, 1083, 1090, 1077, 33, 32, 1084, 1080, 1088, 33]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_06b true(A=='Здравствулте! мир!') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_07a true(L==[20320, 22909, 19990, 30028, 33]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_07b true(A=='你好世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_08a true(L==[127001, 127002, 127003, 127004, 127005, 127006, 127007, 127008]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_08b true(A== 🀙🀚🀛🀜🀝🀞🀟🀠) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_09a true(L==[193, 192, 195, 194, 196, 225, 224, 227, 226, 228]) load
unicode/builtins/tests.lgt lgt_unicode_atom_codes_2_09b true(A=='ÁÀÃÂÄáàãâä') load
unicode/builtins/tests.lgt sics_unicode_atom_codes_2_12 true(C==[80, 233, 99, 115]) load
unicode/builtins/tests.lgt sics_unicode_atom_codes_2_13 true(A=='Pécs') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_01 true(A=='こんにちは世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_02 true(A=='こんにちは世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_03 true(A=='こんにちは世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_04 true(A=='Здравствулте! мир!') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_05 true(A==' мир!') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_06 true(A=='Здравствулт') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_07 true(A== 🀙🀚🀛🀜🀝🀞🀟🀠) load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_08 true(A=='ÁÀÃÂÄáàãâä') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_09 true(A=='你好世界!') load
unicode/builtins/tests.lgt lgt_unicode_atom_concat_3_10 true(A==!) load
unicode/builtins/tests.lgt sics_unicode_atom_concat_3_11 true(N=='Bartók Béla') load
unicode/builtins/tests.lgt sics_unicode_atom_concat_3_12 true(N=='Bartók ') load
unicode/builtins/tests.lgt sics_unicode_atom_concat_3_13 true(N=='Béla') load
unicode/builtins/tests.lgt sics_unicode_atom_concat_3_14 true(L==[''-'Pécs', 'P'-écs, 'Pé'-cs, 'Péc'-s, 'Pécs'-'']) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_01 true(N==15) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_02 true(N==12) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_03 true(N==8) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_04 true(N==8) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_05 true(N==10) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_06 true(N==18) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_07 true(N==5) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_08 true(N==8) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_09 true(N==10) load
unicode/builtins/tests.lgt sics_unicode_atom_length_2_10 true(N==11) load
unicode/builtins/tests.lgt lgt_unicode_atom_length_2_11 true(N==4) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_01 true(Code==243) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_02 true(Code==233) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_03 true(Char=='Γ') load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_04 true(Char== ¡) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_05 true(Char==こ) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_06 true(Char==여) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_07 true(Char==á) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_08 true(Char=='З') load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_09 true(Char==你) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_10 true(Char== 🀙) load
unicode/builtins/tests.lgt lgt_unicode_char_code_2_11 true(Char=='Á') load
unicode/builtins/tests.lgt lgt_unicode_current_prolog_flag_2_01 true(atom(Encoding)) load
unicode/builtins/tests.lgt lgt_unicode_current_prolog_flag_2_02 true(valid(Encoding)) load
unicode/builtins/tests.lgt lgt_unicode_get_byte_2_01 true(Byte==239) load
unicode/builtins/tests.lgt lgt_unicode_get_char_2_01a true(Char=='Γ') load
unicode/builtins/tests.lgt lgt_unicode_get_char_2_01b true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_get_char_2_02a true(Char==你) load
unicode/builtins/tests.lgt lgt_unicode_get_char_2_02b true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_get_char_2_03 true(Char==a) load
unicode/builtins/tests.lgt lgt_unicode_get_code_2_01a true(Code==915) load
unicode/builtins/tests.lgt lgt_unicode_get_code_2_01b true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_get_code_2_02a true(Code==20320) load
unicode/builtins/tests.lgt lgt_unicode_get_code_2_02b true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_open_4_01 true load
unicode/builtins/tests.lgt lgt_unicode_open_4_02 true load
unicode/builtins/tests.lgt lgt_unicode_open_4_03 true load
unicode/builtins/tests.lgt lgt_unicode_open_4_04 true(Byte==239) load
unicode/builtins/tests.lgt lgt_unicode_open_4_05 true(Byte==97) load
unicode/builtins/tests.lgt lgt_unicode_open_4_06 true(Byte==97) load
unicode/builtins/tests.lgt lgt_unicode_open_4_07 true(Byte1-Byte2==97-98) load
unicode/builtins/tests.lgt lgt_unicode_open_4_08 true(Encoding==Default) load
unicode/builtins/tests.lgt lgt_unicode_peek_char_2_01 true(Char=='Γ') load
unicode/builtins/tests.lgt lgt_unicode_peek_char_2_02 true(Char==你) load
unicode/builtins/tests.lgt lgt_unicode_peek_code_2_01 true(Code==915) load
unicode/builtins/tests.lgt lgt_unicode_peek_code_2_02 true(Code==20320) load
unicode/builtins/tests.lgt lgt_unicode_put_char_2_01 true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_put_char_2_02 true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_put_code_2_01 true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_put_code_2_02 true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_read_term_3_01 true(Term=='Γειά σου κόσμε!') load
unicode/builtins/tests.lgt lgt_unicode_read_term_3_02 true(Term=='你好世界!') load
unicode/builtins/tests.lgt lgt_unicode_set_prolog_flag_2_01 true load
unicode/builtins/tests.lgt lgt_unicode_set_stream_position_2_01 true(Term1==Term3) load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_01 true(Encoding-BOM=='UTF-8'-false) load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_02 true(Encoding-BOM=='UTF-8'-false) load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_03 true(Encoding-BOM=='UTF-8'-true) load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_04 true(Encoding=='UTF-8') load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_05 true(BOM==false) load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_06 true(Encoding-BOM=='UTF-8'-true) load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_07 true load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_08 true load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_09 true load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_10 true load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_11 true load
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_12 true load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_01 true(L==[0-13-'Γε', 1-12-ει, 2-11-ιά, 3-10-'ά ', 4-9-' σ', 5-8-σο, 6-7-ου, 7-6-'υ ', 8-5-' κ', 9-4-κό, 10-3-όσ, 11-2-σμ, 12-1-με, 13-0-'ε!']) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_02 true(L==[0-10-'¡H', 1-9-'Ho', 2-8-ol, 3-7-la, 4-6-'a ', 5-5-' m', 6-4-mu, 7-3-un, 8-2-nd, 9-1-do, 10-0-'o!']) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_03 true(L==[0-6-こん, 1-5-んに, 2-4-にち, 3-3-ちは, 4-2-は世, 5-1-世界, 6-0-'界!']) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_04 true(L==[0-6-여보, 1-5-보세, 2-4-세요, 3-3-'요 ', 4-2-' 세', 5-1-세계, 6-0-'계!']) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_05 true(L==[0-8-'Ol', 1-7-lá, 2-6-'á ', 3-5-' m', 4-4-mu, 5-3-un, 6-2-nd, 7-1-do, 8-0-'o!']) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_06 true(L==[0-16-'Зд', 1-15-др, 2-14-ра, 3-13-ав, 4-12-вс, 5-11-ст, 6-10-тв, 7-9-ву, 8-8-ул, 9-7-лт, 10-6-те, 11-5-'е!', 12-4-'! ', 13-3-' м', 14-2-ми, 15-1-ир, 16-0-'р!']) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_07 true(L==[0-3-你好, 1-2-好世, 2-1-世界, 3-0-'界!']) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_08 true(L==[3-矢量]) load
unicode/builtins/tests.lgt lgt_unicode_sub_atom_5_09 true(L==[0-3-矢量, 1-2-'量 ', 2-1-' 空', 3-0-空间]) load
unicode/builtins/tests.lgt sics_unicode_sub_atom_5_31 true(A-S==5-ók) load
unicode/builtins/tests.lgt sics_unicode_sub_atom_5_32 true(L-S==2-ók) load
unicode/builtins/tests.lgt sics_unicode_sub_atom_5_33 true(B-S==4-ók) load
unicode/builtins/tests.lgt sics_unicode_sub_atom_5_34 true(L==[0-2-'Pé', 1-1-éc, 2-0-cs]) load
unicode/builtins/tests.lgt sics_unicode_sub_atom_5_35 true(L==[0-4-7, 7-4-0]) load
unicode/builtins/tests.lgt lgt_unicode_write_term_3_01 true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_write_term_3_02 true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_format_2_01 true(Assertion) load
unicode/builtins/tests.lgt lgt_unicode_format_3_01 true(Assertion) load
unicode/case_variables/tests.lgt lgt_unicode_atom_writing_lower_case_start true(Assertion) load
unicode/case_variables/tests.lgt lgt_unicode_atom_writing_lower_case_start_quoted true(Assertion) load
unicode/case_variables/tests.lgt lgt_unicode_atom_writing_upper_case_start true(Assertion) load
unicode/case_variables/tests.lgt lgt_unicode_atom_writing_upper_case_start_quoted true(Assertion) load
unicode/case_variables/tests.lgt lgt_unicode_atom_reading_lower_case_start true(atom(Term)) load
unicode/case_variables/tests.lgt lgt_unicode_atom_reading_upper_case_start true(atom(Term)) load
unicode/case_variables/tests.lgt lgt_unicode_variable_reading true(var(Term)) load
unicode/case_variables/tests.lgt lgt_unicode_variable_unifying true({Term1=Term2}) load
unicode/case_variables/tests.lgt lgt_unicode_ground_compound_term_writing true(Assertion) load
unicode/case_variables/tests.lgt lgt_unicode_ground_compound_term_reading true(Term=..[αβγ, 'Γβα']) load
unicode/case_variables/tests.lgt lgt_unicode_non_ground_compound_term_writing true(Assertion) load
unicode/case_variables/tests.lgt lgt_unicode_non_ground_compound_term_reading variant(VariableNames, ['Γβα'=_]) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_bmp_01 true(L==['Γ', ε, ι, ά, ' ', σ, ο, υ, ' ', κ, ό, σ, μ, ε, !]) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_bmp_02 true(A=='Γειά σου κόσμε!') load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_bmp_03 true(L==[915, 949, 953, 940, 32, 963, 959, 965, 32, 954, 972, 963, 956, 949, 33]) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_bmp_04 true(A=='Γειά σου κόσμε!') load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_bmp_05 true(Assertion) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_bmp_06 error(syntax_error(_)) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_full_01 true(L==[🀙, 🀚, 🀛, 🀜, 🀝, 🀞, 🀟, 🀠]) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_full_02 true(A== 🀙🀚🀛🀜🀝🀞🀟🀠) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_full_03 true(L==[127001, 127002, 127003, 127004, 127005, 127006, 127007, 127008]) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_full_04 true(A== 🀙🀚🀛🀜🀝🀞🀟🀠) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_full_05 true(Assertion) load
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_full_06 error(syntax_error(_)) load
別欄:
control/throw_1 lgt_throw_1_03
control/throw_1 lgt_throw_1_03
directives/conditional_compilation 39
functions/acosh_1 6
functions/asinh_1 5
functions/atanh_1 9
functions/bitwise_and_2 iso_bitwise_and_2_04
functions/bitwise_complement_1 iso_bitwise_complement_1_03
functions/bitwise_left_shift_2 iso_bitwise_left_shift_2_03
functions/bitwise_or_2 iso_bitwise_or_2_04
functions/bitwise_right_shift_2 iso_bitwise_right_shift_2_03
functions/cosh_1 6
functions/log10_1 9
functions/lsb_1 12
functions/msb_1 lgt_msb_1_07
functions/msb_1 lgt_msb_1_08
functions/popcount_1 11
functions/sinh_1 6
functions/tanh_1 5
functions/xor_2 iso_xor_2_03
predicates/acyclic_term_1 4
predicates/arg_3 iso_arg_3_12
predicates/arg_3 iso_arg_3_12
predicates/arg_3 lgt_arg_3_24
predicates/arg_3 lgt_arg_3_24
predicates/atom_chars_2 sics_atom_chars_2_14
predicates/atom_chars_2 sics_atom_chars_2_15
predicates/atom_codes_2 sics_atom_codes_2_12
predicates/atom_codes_2 sics_atom_codes_2_13
predicates/atom_concat_3 sics_atom_concat_3_11
predicates/atom_concat_3 sics_atom_concat_3_12
predicates/atom_concat_3 sics_atom_concat_3_13
predicates/atom_concat_3 sics_atom_concat_3_14
predicates/atom_length_2 sics_atom_length_2_09
predicates/atomic_concat_3 8
predicates/atomic_list_concat_2 7
predicates/atomic_list_concat_3 10
predicates/bagof_3 iso_bagof_3_09
predicates/bagof_3 lgt_bagof_3_22
predicates/bagof_3 lgt_bagof_3_22
predicates/between_3 13
predicates/call_nth_2 11
predicates/clause_2 iso_clause_2_11
predicates/clause_2 iso_clause_2_11
predicates/clause_2 lgt_clause_2_14
predicates/clause_2 lgt_clause_2_14
predicates/clause_2 lgt_clause_2_15
predicates/clause_2 lgt_clause_2_15
predicates/clause_2 lgt_clause_2_16
predicates/clause_2 lgt_clause_2_16
predicates/clause_references 26
predicates/compare_3 lgt_compare_3_30
predicates/compare_3 lgt_compare_3_30
predicates/copy_term_2 iso_copy_term_2_09
predicates/copy_term_2 iso_copy_term_2_09
predicates/copy_term_2 lgt_copy_term_2_12
predicates/copy_term_2 lgt_copy_term_2_12
predicates/copy_term_2 lgt_copy_term_2_13
predicates/copy_term_2 lgt_copy_term_2_13
predicates/copy_term_2 lgt_copy_term_2_14
predicates/copy_term_2 lgt_copy_term_2_14
predicates/current_prolog_flag_2 lgt_current_prolog_flag_2_21
predicates/evaluable_property_2 16
predicates/findall_3 lgt_findall_3_11
predicates/findall_3 lgt_findall_3_11
predicates/forall_2 0
predicates/format_2 205
predicates/format_3 207
predicates/functor_3 lgt_functor_3_20
predicates/functor_3 lgt_functor_3_20
predicates/functor_3 lgt_functor_3_21
predicates/functor_3 lgt_functor_3_21
predicates/ground_1 9
predicates/keysort_2 23
predicates/length_2 29
predicates/msort_2 16
predicates/nl_1 lgt_nl_0_01
predicates/nl_1 lgt_nl_1_07
predicates/not_1 iso_not_1_08
predicates/not_1 iso_not_1_08
predicates/not_unifiable_2 20
predicates/number_chars_2 sics_number_chars_2_28
predicates/numbervars_3 19
predicates/once_1 iso_once_1_05
predicates/once_1 iso_once_1_05
predicates/op_3 iso_op_3_10
predicates/op_3 iso_op_3_20
predicates/op_3 iso_op_3_21
predicates/open_4 lgt_open_4_27
predicates/open_4 lgt_open_4_28
predicates/open_4 lgt_open_4_29
predicates/open_4 lgt_open_4_30
predicates/open_4 lgt_open_4_40
predicates/plus_3 16
predicates/portray_1 2
predicates/predicate_property_2 29
predicates/retract_1 iso_retract_1_07
predicates/retract_1 iso_retract_1_07
predicates/set_prolog_flag_2 lgt_set_prolog_flag_2_14
predicates/set_prolog_flag_2 lgt_set_prolog_flag_2_15
predicates/set_prolog_flag_2 lgt_set_prolog_flag_2_16
predicates/set_prolog_flag_2 lgt_set_prolog_flag_2_17
predicates/set_prolog_flag_2 lgt_set_prolog_flag_2_18
predicates/set_prolog_flag_2 lgt_set_prolog_flag_2_19
predicates/setof_3 eddbali_setof_3_25
predicates/setof_3 eddbali_setof_3_25
predicates/setof_3 iso_setof_3_11
predicates/setof_3 lgt_setof_3_36
predicates/setof_3 lgt_setof_3_36
predicates/setup_call_cleanup_3 31
predicates/sort_2 21
predicates/stream_property_2 lgt_stream_property_2_09b
predicates/stream_property_2 lgt_stream_property_2_10b
predicates/stream_property_2 sics_stream_property_2_06c
predicates/sub_atom_5 sics_sub_atom_5_29
predicates/sub_atom_5 sics_sub_atom_5_30
predicates/sub_atom_5 sics_sub_atom_5_31
predicates/sub_atom_5 sics_sub_atom_5_32
predicates/subsumes_term_2 20
predicates/succ_2 11
predicates/term_comparison lgt_term_comparison_38
predicates/term_comparison lgt_term_comparison_38
predicates/term_comparison lgt_term_comparison_39
predicates/term_comparison lgt_term_comparison_39
predicates/term_comparison lgt_term_comparison_40
predicates/term_comparison lgt_term_comparison_40
predicates/term_comparison lgt_term_comparison_41
predicates/term_comparison lgt_term_comparison_41
predicates/term_comparison lgt_term_comparison_42
predicates/term_comparison lgt_term_comparison_42
predicates/term_comparison lgt_term_comparison_43
predicates/term_comparison lgt_term_comparison_43
predicates/term_comparison lgt_term_comparison_44
predicates/term_comparison lgt_term_comparison_44
predicates/term_comparison lgt_term_comparison_45
predicates/term_comparison lgt_term_comparison_45
predicates/term_comparison lgt_term_comparison_46
predicates/term_comparison lgt_term_comparison_46
predicates/term_comparison lgt_term_comparison_47
predicates/term_comparison lgt_term_comparison_47
predicates/term_variables_2 iso_term_variables_2_09
predicates/term_variables_2 iso_term_variables_2_09
predicates/term_variables_2 lgt_term_variables_2_14
predicates/term_variables_2 lgt_term_variables_2_14
predicates/term_variables_2 lgt_term_variables_2_15
predicates/term_variables_2 lgt_term_variables_2_15
predicates/unify_2 iso_unify_2_12
predicates/unify_2 iso_unify_2_12
predicates/unify_2 iso_unify_2_13
predicates/unify_2 iso_unify_2_13
predicates/unify_2 iso_unify_2_14
predicates/unify_2 iso_unify_2_14
predicates/unify_2 iso_unify_2_15
predicates/unify_2 iso_unify_2_15
predicates/unify_2 iso_unify_2_16
predicates/unify_2 iso_unify_2_16
predicates/unify_2 iso_unify_2_17
predicates/unify_2 iso_unify_2_17
predicates/unify_with_occurs_check_2 lgt_unify_with_occurs_check_2_18
predicates/unify_with_occurs_check_2 lgt_unify_with_occurs_check_2_18
predicates/univ_2 iso_univ_2_15
predicates/univ_2 iso_univ_2_15
predicates/univ_2 lgt_univ_2_31
predicates/univ_2 lgt_univ_2_31
predicates/univ_2 lgt_univ_2_32
predicates/univ_2 lgt_univ_2_32
predicates/univ_2 lgt_univ_2_33
predicates/univ_2 lgt_univ_2_33
predicates/write_term_3 lgt_write_term_3_123
predicates/write_term_3 lgt_write_term_3_124
predicates/write_term_3 lgt_write_term_3_125
predicates/write_term_3 lgt_write_term_3_126
predicates/write_term_3 lgt_write_term_3_127
predicates/write_term_3 lgt_write_term_3_128
predicates/write_term_3 lgt_write_term_3_129
predicates/write_term_3 lgt_write_term_3_130
predicates/write_term_3 lgt_write_term_3_131
predicates/write_term_3 lgt_write_term_3_132
predicates/write_term_3 lgt_write_term_3_133
predicates/write_term_3 lgt_write_term_3_134
predicates/write_term_3 lgt_write_term_3_135
predicates/write_term_3 lgt_write_term_3_136
predicates/write_term_3 lgt_write_term_3_137
predicates/write_term_3 lgt_write_term_3_138
predicates/write_term_3 lgt_write_term_3_139
predicates/write_term_3 lgt_write_term_3_140
predicates/write_term_3 lgt_write_term_3_141
predicates/write_term_3 lgt_write_term_3_142
predicates/write_term_3 lgt_write_term_3_143
predicates/write_term_3 lgt_write_term_3_144
predicates/write_term_3 lgt_write_term_3_145
syntax/directives/meta_predicate_1 3
syntax/operators lgt_operators_57
unbounded 111
unicode/directives/encoding_1 3
unicode/encodings 49
