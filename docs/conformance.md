このファイルは自動生成である。手で編集しない。
分母: 2326
合格数: 2291
合格率: 98.5
不合格:
control/catch_3/tests.lgt lgt_catch_3_13 true(Assertion) existence_error(procedure,ignore/1)
control/soft_cut_2_3/tests.lgt lgt_soft_cut_2_3_22 true(Deterministic==false) existence_error(procedure,:: /2)
logical_update_semantics/tests.lgt logical_update_semantics_negation true(L==[2,4,6]) existence_error(procedure,:: /2)
predicates/current_input_1/tests.lgt sics_current_input_1_02 error(domain_error(stream,foo)) domain_error(stream_or_alias,foo)
predicates/current_output_1/tests.lgt sics_current_output_1_02 error(domain_error(stream,foo)) domain_error(stream_or_alias,foo)
predicates/current_prolog_flag_2/tests.lgt eddbali_current_prolog_flag_2_12 true domain_error(flag_value,unknown+warning)
predicates/current_prolog_flag_2/tests.lgt eddbali_current_prolog_flag_2_13 false domain_error(flag_value,unknown+warning)
predicates/open_4/tests.lgt lgt_open_4_31 errors([permission_error(open,source_sink,_),existence_error(source_sink,_)]) unsupported
predicates/open_4/tests.lgt lgt_open_4_41 true(Action==eof_code) existence_error(procedure,:: /2)
predicates/open_4/tests.lgt lgt_open_4_42 true(Action==reset) existence_error(procedure,:: /2)
predicates/open_4/tests.lgt lgt_open_4_43 true(Action==error) existence_error(procedure,:: /2)
predicates/set_stream_position_2/tests.lgt sics_set_stream_position_2_06 error(permission_error(reposition,stream,Stream)) fail
predicates/stream_property_2/tests.lgt sics_stream_property_2_03 error(domain_error(stream,foo)) domain_error(stream_or_alias,foo)
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
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_11 true existence_error(source_sink,'tests/tmp/suite/unicode/builtins/sample_utf_8_bom')
unicode/builtins/tests.lgt lgt_unicode_stream_property_2_12 true existence_error(source_sink,'tests/tmp/suite/unicode/builtins/sample_utf_8_bom')
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
functions/div_2 lgt_div_2_10
functions/integer_division_2 lgt_integer_division_2_12
functions/integer_power_2 lgt_integer_power_2_18
functions/log10_1 9
functions/lsb_1 12
functions/minus_1 lgt_minus_1_07
functions/minus_2 lgt_minus_2_09
functions/multiplication_2 lgt_multiplication_2_13
functions/plus_2 lgt_plus_2_09
functions/popcount_1 11
functions/sinh_1 6
functions/tanh_1 5
functions/xor_2 iso_xor_2_03
predicates/acyclic_term_1 iso_acyclic_term_1_02
predicates/acyclic_term_1 iso_acyclic_term_1_02
predicates/acyclic_term_1 lgt_acyclic_term_1_03
predicates/acyclic_term_1 lgt_acyclic_term_1_03
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
predicates/bagof_3 iso_bagof_3_09
predicates/bagof_3 lgt_bagof_3_22
predicates/bagof_3 lgt_bagof_3_22
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
predicates/format_2 lgt_format_2_new_line
predicates/format_2 lgt_format_2_new_line_if_not_beginning_of_line
predicates/format_2 lgt_format_2_new_line_if_not_beginning_of_line_ignore_count
predicates/format_2 lgt_format_2_new_line_n
predicates/format_2 lgt_format_2_tab_table_left_aligned
predicates/format_2 lgt_format_2_tab_table_pip_0110_23
predicates/format_2 lgt_format_2_tab_table_right_aligned
predicates/format_3 lgt_format_3_new_line
predicates/format_3 lgt_format_3_new_line_if_not_beginning_of_line
predicates/format_3 lgt_format_3_new_line_if_not_beginning_of_line_ignore_count
predicates/format_3 lgt_format_3_new_line_n
predicates/format_3 lgt_format_3_tab_table_left_aligned
predicates/format_3 lgt_format_3_tab_table_pip_0110_23
predicates/format_3 lgt_format_3_tab_table_right_aligned
predicates/functor_3 lgt_functor_3_20
predicates/functor_3 lgt_functor_3_20
predicates/functor_3 lgt_functor_3_21
predicates/functor_3 lgt_functor_3_21
predicates/ground_1 lgt_ground_1_08
predicates/ground_1 lgt_ground_1_08
predicates/is_2 iso_is_2_31
predicates/is_2 iso_is_2_32
predicates/is_2 iso_is_2_33
predicates/is_2 iso_is_2_34
predicates/is_2 iso_is_2_35
predicates/is_2 lgt_is_2_44
predicates/is_2 lgt_is_2_45
predicates/is_2 lgt_is_2_46
predicates/keysort_2 iso_keysort_2_04
predicates/keysort_2 iso_keysort_2_05
predicates/keysort_2 iso_keysort_2_05
predicates/keysort_2 lgt_keysort_2_21
predicates/keysort_2 lgt_keysort_2_21
predicates/length_2 commons_length_2_25
predicates/length_2 commons_length_2_26
predicates/length_2 commons_length_2_26
predicates/length_2 commons_length_2_27
predicates/length_2 commons_length_2_27
predicates/msort_2 lgt_msort_2_04
predicates/msort_2 lgt_msort_2_04
predicates/nl_1 lgt_nl_0_01
predicates/nl_1 lgt_nl_1_07
predicates/not_1 iso_not_1_08
predicates/not_1 iso_not_1_08
predicates/not_unifiable_2 iso_not_unifiable_2_11
predicates/not_unifiable_2 iso_not_unifiable_2_11
predicates/not_unifiable_2 iso_not_unifiable_2_12
predicates/not_unifiable_2 iso_not_unifiable_2_12
predicates/not_unifiable_2 iso_not_unifiable_2_13
predicates/not_unifiable_2 iso_not_unifiable_2_13
predicates/not_unifiable_2 iso_not_unifiable_2_14
predicates/not_unifiable_2 iso_not_unifiable_2_14
predicates/not_unifiable_2 iso_not_unifiable_2_15
predicates/not_unifiable_2 iso_not_unifiable_2_15
predicates/number_chars_2 sics_number_chars_2_28
predicates/numbervars_3 lgt_numbervars_3_16
predicates/numbervars_3 lgt_numbervars_3_16
predicates/numbervars_3 lgt_numbervars_3_17
predicates/numbervars_3 lgt_numbervars_3_17
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
predicates/plus_3 lgt_plus_3_16
predicates/portray_1 2
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
predicates/sort_2 iso_sort_2_04
predicates/sort_2 iso_sort_2_04
predicates/sort_2 lgt_sort_2_18
predicates/sort_2 lgt_sort_2_18
predicates/stream_property_2 lgt_stream_property_2_09b
predicates/stream_property_2 lgt_stream_property_2_10b
predicates/stream_property_2 sics_stream_property_2_06c
predicates/sub_atom_5 sics_sub_atom_5_29
predicates/sub_atom_5 sics_sub_atom_5_30
predicates/sub_atom_5 sics_sub_atom_5_31
predicates/sub_atom_5 sics_sub_atom_5_32
predicates/subsumes_term_2 lgt_subsumes_term_2_15
predicates/subsumes_term_2 lgt_subsumes_term_2_15
predicates/subsumes_term_2 lgt_subsumes_term_2_16
predicates/subsumes_term_2 lgt_subsumes_term_2_16
predicates/subsumes_term_2 lgt_subsumes_term_2_17
predicates/subsumes_term_2 lgt_subsumes_term_2_17
predicates/succ_2 lgt_succ_2_11
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
unbounded lgt_unbounded_arg_02
unbounded lgt_unbounded_arg_03
unbounded lgt_unbounded_functor_01
unbounded lgt_unbounded_functor_02
unbounded lgt_unbounded_plus_01
unbounded lgt_unbounded_plus_02
unbounded lgt_unbounded_plus_03
unbounded lgt_unbounded_succ_01
unbounded lgt_unbounded_succ_02
unicode/builtins lgt_unicode_format_2_01
unicode/directives/encoding_1 3
unicode/encodings 49
ライブラリ欄の分母: 566
ライブラリ欄の合格数: 378
ライブラリ欄の合格率: 66.8
ライブラリ欄の不合格:
predicates/format_2/tests.lgt lgt_format_2_canonical_operators true(Assertion) domain_error(format_directive,k)
predicates/format_2/tests.lgt lgt_format_2_canonical_quotes true(Assertion) domain_error(format_directive,k)
predicates/format_2/tests.lgt lgt_format_2_print_default true(Assertion) domain_error(format_directive,p)
predicates/format_2/tests.lgt lgt_format_2_print_portray_01 true(Assertion) domain_error(format_directive,p)
predicates/format_2/tests.lgt lgt_format_2_print_portray_02 true(Assertion) domain_error(format_directive,p)
predicates/format_2/tests.lgt lgt_format_2_print_portray_03 true(Assertion) domain_error(format_directive,p)
predicates/format_2/tests.lgt lgt_format_2_print_portray_04 true(Assertion) domain_error(format_directive,p)
predicates/format_2/tests.lgt lgt_format_2_new_line_if_not_beginning_of_line true(Assertion) domain_error(format_directive,'N')
predicates/format_2/tests.lgt lgt_format_2_new_line_if_not_beginning_of_line_ignore_count true(Assertion) domain_error(format_directive,'N')
predicates/format_2/tests.lgt lgt_format_2_ignore true(Assertion) domain_error(format_directive,i)
predicates/format_2/tests.lgt lgt_format_2_ignore_fixed_count true(Assertion) domain_error(format_directive,i)
predicates/format_2/tests.lgt lgt_format_2_ignore_start_count true(Assertion) domain_error(format_directive,i)
predicates/format_2/tests.lgt lgt_format_2_ignore_expression_count true(Assertion) domain_error(format_directive,i)
predicates/format_2/tests.lgt lgt_format_2_decimal_group true(Assertion) domain_error(format_directive,'D')
predicates/format_2/tests.lgt lgt_format_2_decimal_group_n true(Assertion) domain_error(format_directive,'D')
predicates/format_2/tests.lgt lgt_format_2_decimal_group_star true(Assertion) domain_error(format_directive,'D')
predicates/format_2/tests.lgt lgt_format_2_decimal_invalid_02 error(instantiation_error) domain_error(format_directive,'D')
predicates/format_2/tests.lgt lgt_format_2_decimal_invalid_08 error(type_error(integer,123.0)) domain_error(format_directive,'D')
predicates/format_2/tests.lgt lgt_format_2_decimal_invalid_10 error(type_error(evaluable,foo/1)) domain_error(format_directive,'D')
predicates/format_2/tests.lgt lgt_format_2_radix_2 true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_8 true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_10 true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_16 true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_36 true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_36_expression true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_star true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_upper_case_2 true(Assertion) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_upper_case_8 true(Assertion) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_upper_case_16 true(Assertion) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_upper_case_36 true(Assertion) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_upper_case_36_expression true(Assertion) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_upper_case_star true(Assertion) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_default true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_default_upper_case true(Assertion) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_invalid_01 error(instantiation_error) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_invalid_02 error(instantiation_error) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_invalid_07 error(type_error(integer,123.0)) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_invalid_08 error(type_error(integer,123.0)) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_radix_invalid_09 error(type_error(evaluable,foo/1)) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_radix_invalid_10 error(type_error(evaluable,foo/1)) domain_error(format_directive,'R')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_upper_case true(Assertion) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_upper_case_expression true(Assertion) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_n_places_upper_case true(Assertion) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_upper_case_star_places true(Assertion) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_upper_case_zero_places true(Assertion) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_upper_case_invalid_01 error(instantiation_error) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_upper_case_invalid_03 error(instantiation_error) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_exponential_notation_upper_case_invalid_04 error(type_error(evaluable,foo/1)) domain_error(format_directive,'E')
predicates/format_2/tests.lgt lgt_format_2_float_best_e true(Assertion) domain_error(format_directive,g)
predicates/format_2/tests.lgt lgt_format_2_float_best_ne true(Assertion) domain_error(format_directive,g)
predicates/format_2/tests.lgt lgt_format_2_float_best_f true(Assertion) domain_error(format_directive,g)
predicates/format_2/tests.lgt lgt_format_2_float_best_nf true(Assertion) domain_error(format_directive,g)
predicates/format_2/tests.lgt lgt_format_2_float_best_invalid_01 error(instantiation_error) domain_error(format_directive,g)
predicates/format_2/tests.lgt lgt_format_2_float_best_invalid_03 error(instantiation_error) domain_error(format_directive,g)
predicates/format_2/tests.lgt lgt_format_2_float_best_invalid_04 error(type_error(evaluable,foo/1)) domain_error(format_directive,g)
predicates/format_2/tests.lgt lgt_format_2_float_best_e_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_2/tests.lgt lgt_format_2_float_best_ne_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_2/tests.lgt lgt_format_2_float_best_f_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_2/tests.lgt lgt_format_2_float_best_nf_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_2/tests.lgt lgt_format_2_float_best_upper_case_invalid_01 error(instantiation_error) domain_error(format_directive,'G')
predicates/format_2/tests.lgt lgt_format_2_float_best_upper_case_invalid_03 error(instantiation_error) domain_error(format_directive,'G')
predicates/format_2/tests.lgt lgt_format_2_float_best_upper_case_invalid_04 error(type_error(evaluable,foo/1)) domain_error(format_directive,'G')
predicates/format_2/tests.lgt lgt_format_2_tab_atom_left_aligned_default_column_boundary true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_atom_left_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_integer_left_aligned_default_column_boundary true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_integer_left_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_atom_center_aligned_default_column_boundary true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_atom_center_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_integer_center_aligned_default_column_boundary true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_integer_center_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_all_alignments_in true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_all_alignments_out true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_left_aligned true(Assertion) existence_error(procedure,:: /2)
predicates/format_2/tests.lgt lgt_format_2_tab_table_right_aligned true(Assertion) existence_error(procedure,:: /2)
predicates/format_2/tests.lgt lgt_format_2_tab_table_fill_character true(Assertion) domain_error(format_directive,r)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_06 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_07 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_08 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_09 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_10 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_12 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_14 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_15 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_16 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_17 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_18 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_19 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_20 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_21 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_22 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_tab_table_pip_0110_23 true(Assertion) domain_error(format_directive,+)
predicates/format_2/tests.lgt lgt_format_2_write_term_default true(Assertion) domain_error(format_directive,'W')
predicates/format_2/tests.lgt lgt_format_2_write_term_quoted true(Assertion) domain_error(format_directive,'W')
predicates/format_2/tests.lgt lgt_format_2_write_term_canonical true(Assertion) domain_error(format_directive,'W')
predicates/format_3/tests.lgt lgt_format_3_canonical_operators true(Assertion) domain_error(format_directive,k)
predicates/format_3/tests.lgt lgt_format_3_canonical_quotes true(Assertion) domain_error(format_directive,k)
predicates/format_3/tests.lgt lgt_format_3_print_default true(Assertion) domain_error(format_directive,p)
predicates/format_3/tests.lgt lgt_format_3_print_portray_01 true(Assertion) domain_error(format_directive,p)
predicates/format_3/tests.lgt lgt_format_3_print_portray_02 true(Assertion) domain_error(format_directive,p)
predicates/format_3/tests.lgt lgt_format_3_print_portray_03 true(Assertion) domain_error(format_directive,p)
predicates/format_3/tests.lgt lgt_format_3_print_portray_04 true(Assertion) domain_error(format_directive,p)
predicates/format_3/tests.lgt lgt_format_3_new_line_if_not_beginning_of_line true(Assertion) domain_error(format_directive,'N')
predicates/format_3/tests.lgt lgt_format_3_new_line_if_not_beginning_of_line_ignore_count true(Assertion) domain_error(format_directive,'N')
predicates/format_3/tests.lgt lgt_format_3_ignore true(Assertion) domain_error(format_directive,i)
predicates/format_3/tests.lgt lgt_format_3_ignore_fixed_count true(Assertion) domain_error(format_directive,i)
predicates/format_3/tests.lgt lgt_format_3_ignore_star_count true(Assertion) domain_error(format_directive,i)
predicates/format_3/tests.lgt lgt_format_3_ignore_expresion_count true(Assertion) domain_error(format_directive,i)
predicates/format_3/tests.lgt lgt_format_3_decimal_group true(Assertion) domain_error(format_directive,'D')
predicates/format_3/tests.lgt lgt_format_3_decimal_group_n true(Assertion) domain_error(format_directive,'D')
predicates/format_3/tests.lgt lgt_format_3_decimal_group_star true(Assertion) domain_error(format_directive,'D')
predicates/format_3/tests.lgt lgt_format_3_decimal_invalid_02 error(instantiation_error) domain_error(format_directive,'D')
predicates/format_3/tests.lgt lgt_format_3_decimal_invalid_08 error(type_error(integer,123.0)) domain_error(format_directive,'D')
predicates/format_3/tests.lgt lgt_format_3_decimal_invalid_10 error(type_error(evaluable,foo/1)) domain_error(format_directive,'D')
predicates/format_3/tests.lgt lgt_format_3_radix_2 true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_8 true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_10 true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_16 true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_36 true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_36_expression true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_star true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_upper_case_2 true(Assertion) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_upper_case_8 true(Assertion) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_upper_case_16 true(Assertion) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_upper_case_36 true(Assertion) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_upper_case_36_expression true(Assertion) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_upper_case_star true(Assertion) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_default true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_default_upper_case true(Assertion) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_invalid_01 error(instantiation_error) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_invalid_02 error(instantiation_error) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_invalid_07 error(type_error(integer,123.0)) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_invalid_08 error(type_error(integer,123.0)) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_radix_invalid_09 error(type_error(evaluable,foo/1)) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_radix_invalid_10 error(type_error(evaluable,foo/1)) domain_error(format_directive,'R')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_upper_case true(Assertion) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_upper_case_expression true(Assertion) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_n_places_upper_case true(Assertion) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_upper_case_star_places true(Assertion) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_upper_case_zero_places true(Assertion) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_upper_case_invalid_01 error(instantiation_error) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_upper_case_invalid_03 error(instantiation_error) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_exponential_notation_upper_case_invalid_04 error(type_error(evaluable,foo/1)) domain_error(format_directive,'E')
predicates/format_3/tests.lgt lgt_format_3_float_best_e true(Assertion) domain_error(format_directive,g)
predicates/format_3/tests.lgt lgt_format_3_float_best_ne true(Assertion) domain_error(format_directive,g)
predicates/format_3/tests.lgt lgt_format_3_float_best_f true(Assertion) domain_error(format_directive,g)
predicates/format_3/tests.lgt lgt_format_3_float_best_nf true(Assertion) domain_error(format_directive,g)
predicates/format_3/tests.lgt lgt_format_3_float_best_invalid_01 error(instantiation_error) domain_error(format_directive,g)
predicates/format_3/tests.lgt lgt_format_3_float_best_invalid_03 error(instantiation_error) domain_error(format_directive,g)
predicates/format_3/tests.lgt lgt_format_3_float_best_invalid_04 error(type_error(evaluable,foo/1)) domain_error(format_directive,g)
predicates/format_3/tests.lgt lgt_format_3_float_best_e_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_3/tests.lgt lgt_format_3_float_best_ne_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_3/tests.lgt lgt_format_3_float_best_f_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_3/tests.lgt lgt_format_3_float_best_nf_upper_case true(Assertion) domain_error(format_directive,'G')
predicates/format_3/tests.lgt lgt_format_3_float_best_upper_case_invalid_01 error(instantiation_error) domain_error(format_directive,'G')
predicates/format_3/tests.lgt lgt_format_3_float_best_upper_case_invalid_03 error(instantiation_error) domain_error(format_directive,'G')
predicates/format_3/tests.lgt lgt_format_3_float_best_upper_case_invalid_04 error(type_error(evaluable,foo/1)) domain_error(format_directive,'G')
predicates/format_3/tests.lgt lgt_format_3_tab_atom_left_aligned_default_column_boundary true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_atom_left_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_integer_left_aligned_default_column_boundary true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_integer_left_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_atom_center_aligned_default_column_boundary true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_atom_center_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_integer_center_aligned true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_all_alignments_in true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_all_alignments_out true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_left_aligned true(Assertion) existence_error(procedure,:: /2)
predicates/format_3/tests.lgt lgt_format_3_tab_table_right_aligned true(Assertion) existence_error(procedure,:: /2)
predicates/format_3/tests.lgt lgt_format_3_tab_table_fill_character true(Assertion) domain_error(format_directive,r)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_06 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_07 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_08 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_09 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_10 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_12 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_14 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_15 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_16 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_17 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_18 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_19 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_20 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_21 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_22 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_tab_table_pip_0110_23 true(Assertion) domain_error(format_directive,+)
predicates/format_3/tests.lgt lgt_format_3_write_term_default true(Assertion) domain_error(format_directive,'W')
predicates/format_3/tests.lgt lgt_format_3_write_term_quoted true(Assertion) domain_error(format_directive,'W')
predicates/format_3/tests.lgt lgt_format_3_write_term_canonical true(Assertion) domain_error(format_directive,'W')
predicates/setup_call_cleanup_3/tests.lgt lgt_setup_call_cleanup_3_26 true(var(Deterministic)) fail
