このファイルは自動生成である。手で編集しない。
分母: 2171
合格数: 2094
合格率: 96.5
不合格:
control/call_1/tests.lgt lgt_call_1_17 false existence_error(procedure,fenix/1)
control/call_1/tests.lgt lgt_call_1_18 false existence_error(procedure,scattered/2)
control/catch_3/tests.lgt lgt_catch_3_13 true(Assertion) existence_error(procedure,ignore/1)
control/if_3/tests.lgt if_3_01 true existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_02 true existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_03 false existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_04 false existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_05 true(L==[1-4,1-5,1-6,2-4,2-5,2-6,3-4,3-5,3-6]) existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_06 true(L==[7,8,9]) existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_07 true(L==[7,8,9]) existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_08 true existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_09 true(L==[1,2]) existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_10 errors([type_error(callable,3),type_error(callable,:(user,3)),type_error(callable,if(3,true,true))]) existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_11 errors([type_error(callable,3),type_error(callable,:(user,3)),type_error(callable,if(true,3,true))]) existence_error(procedure,if/3)
control/if_3/tests.lgt if_3_12 errors([type_error(callable,3),type_error(callable,:(user,3)),type_error(callable,if(fail,true,3))]) existence_error(procedure,if/3)
control/soft_cut_2_3/tests.lgt lgt_soft_cut_2_3_22 true(Deterministic==false) existence_error(procedure,:: /2)
functions/ceiling_1/tests.lgt lgt_ceiling_1_05 error(type_error(float,9)) true
functions/float_fractional_part_1/tests.lgt lgt_float_fractional_part_1_04 error(type_error(float,9)) true
functions/float_integer_part_1/tests.lgt lgt_float_integer_part_1_04 error(type_error(float,9)) true
functions/floor_1/tests.lgt lgt_floor_1_06 error(type_error(float,9)) true
functions/integer_power_2/tests.lgt iso_integer_power_2_06 error(type_error(float,2)) evaluation_error(undefined)
functions/power_2/tests.lgt lgt_power_2_14 errors([evaluation_error(undefined),evaluation_error(zero_divisor)]) evaluation_error(float_overflow)
functions/round_1/tests.lgt lgt_round_1_13 error(type_error(float,9)) true
functions/truncate_1/tests.lgt lgt_truncate_1_06 error(type_error(float,9)) true
logical_update_semantics/tests.lgt logical_update_semantics_retract true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_retractall true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_clause_retract true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_clause_abolish true(Assertion) fail
logical_update_semantics/tests.lgt logical_update_semantics_negation true(L==[2,4,6]) existence_error(procedure,:: /2)
predicates/abolish_1/tests.lgt eddbali_abolish_1_07 true(L==[ant,bee]) fail
predicates/current_input_1/tests.lgt sics_current_input_1_02 error(domain_error(stream,foo)) domain_error(stream_or_alias,foo)
predicates/current_input_1/tests.lgt sics_current_input_1_04 error(domain_error(stream,S)) existence_error(stream,'$stream'(3))
predicates/current_output_1/tests.lgt sics_current_output_1_02 error(domain_error(stream,foo)) domain_error(stream_or_alias,foo)
predicates/current_output_1/tests.lgt sics_current_output_1_04 error(domain_error(stream,S)) existence_error(stream,'$stream'(3))
predicates/current_prolog_flag_2/tests.lgt eddbali_current_prolog_flag_2_12 true domain_error(flag_value,unknown+warning)
predicates/current_prolog_flag_2/tests.lgt eddbali_current_prolog_flag_2_13 false domain_error(flag_value,unknown+warning)
predicates/current_prolog_flag_2/tests.lgt commons_current_prolog_flag_2_16 true(atom(Dialect)) domain_error(prolog_flag,dialect)
predicates/current_prolog_flag_2/tests.lgt commons_current_prolog_flag_2_17 true((integer(Major),integer(Minor),integer(Patch))) domain_error(prolog_flag,version_data)
predicates/current_prolog_flag_2/tests.lgt commons_current_prolog_flag_2_18 true(functor(VersionData,Dialect,_)) domain_error(prolog_flag,dialect)
predicates/open_4/tests.lgt lgt_open_4_31 errors([permission_error(open,source_sink,_),existence_error(source_sink,_)]) unsupported
predicates/open_4/tests.lgt lgt_open_4_41 true(Action==eof_code) existence_error(procedure,:: /2)
predicates/open_4/tests.lgt lgt_open_4_42 true(Action==reset) existence_error(procedure,:: /2)
predicates/open_4/tests.lgt lgt_open_4_43 true(Action==error) existence_error(procedure,:: /2)
predicates/read_term_3/tests.lgt sics_read_term_3_24 true(X==Integer) syntax_error('行 1 列 2: 整数が範囲を超えている 9223372036854775808')
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_06 error(permission_error(reposition,stream,Stream)) fail
predicates/stream_property_2/tests.lgt sics_stream_property_2_03 error(domain_error(stream,foo)) domain_error(stream_or_alias,foo)
predicates/stream_property_2/tests.lgt sics_stream_property_2_05c deterministic(Action==reset) fail
predicates/stream_property_2/tests.lgt sics_stream_property_2_06d deterministic(Mode==append) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_08b true(Action==reset) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_09c true(Mode==append) fail
predicates/stream_property_2/tests.lgt lgt_stream_property_2_10c true(Mode==append) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_043 subsumes(['_',_|_],Chars) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_044 subsumes(['_',_|_],Chars) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_065 subsumes(['_',_|_],Chars) fail
predicates/write_term_3/tests.lgt lgt_write_term_3_066 subsumes(['_',_|_],Chars) fail
unicode/builtins/tests.lgt lgt_unicode_current_prolog_flag_2_02 true(valid(Encoding)) fail
unicode/builtins/tests.lgt lgt_unicode_get_char_2_03 true(Char==a) fail
unicode/builtins/tests.lgt lgt_unicode_open_4_01 true domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_open_4_02 true domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_open_4_03 true domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_open_4_04 true(Byte==239) domain_error(stream_option,bom(true))
unicode/builtins/tests.lgt lgt_unicode_open_4_05 true(Byte==97) domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_open_4_06 true(Byte==97) domain_error(stream_option,bom(true))
unicode/builtins/tests.lgt lgt_unicode_open_4_07 true(Byte1-Byte2==97-98) domain_error(stream_option,bom(false))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_01 true(Encoding-BOM=='UTF-8'-false) domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_02 true(Encoding-BOM=='UTF-8'-false) domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_03 true(Encoding-BOM=='UTF-8'-true) domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_04 true(Encoding=='UTF-8') domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_05 true(BOM==false) domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_06 true(Encoding-BOM=='UTF-8'-true) domain_error(stream_option,encoding('UTF-8'))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_07 true fail
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_08 true domain_error(stream_property,bom(_))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_09 true fail
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_10 true domain_error(stream_property,bom(_))
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_11 true existence_error(source_sink,'/Users/hiro/Projetct/GitHub/mosa-prolog/tests/tmp/suite/unicode/builtins/sample_utf_8_bom')
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_12 true existence_error(source_sink,'/Users/hiro/Projetct/GitHub/mosa-prolog/tests/tmp/suite/unicode/builtins/sample_utf_8_bom')
unicode/escape_sequences/tests.lgt lgt_unicode_escape_sequence_full_05 true(Assertion) fail
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
unicode/builtins lgt_unicode_format_2_01
unicode/directives/encoding_1 3
unicode/encodings 49
