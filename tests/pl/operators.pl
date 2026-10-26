test(op_make, (op(30, xfy, ++), open_string('1 ++ 2.', S), read(S, T), T == ++(1, 2), op(0, xfy, ++)), true).
test(op_assoc, (op(30, xfy, ++), open_string('1 ++ 2 ++ 3.', S), read(S, T), T == ++(1, ++(2, 3)), op(0, xfy, ++)), true).
test(op_write, (op(30, xfy, ++), with_output_to(atom(A), write(++(1, 2))), A == '1++2', op(0, xfy, ++)), true).
test(op_named, (op(700, xfx, pp), with_output_to(atom(A), write(pp(1, 2))), A == '1 pp 2', op(0, xfx, pp)), true).
test(op_replace, (op(30, xfx, pp), op(30, xfy, pp), open_string('1 pp 2 pp 3.', S), read(S, T), T == pp(1, pp(2, 3)), op(0, xfx, pp), op(0, xfy, pp)), true).
test(op_prefix, (op(200, fx, qq), op(200, fy, qq), open_string('qq qq 1.', S), read(S, T), T == qq(qq(1)), op(0, fx, qq), op(0, fy, qq)), true).
test(op_list, (op(30, xfy, [pp, qq]), open_string('1 pp 2.', S), read(S, T), T == pp(1, 2), open_string('3 qq 4.', S2), read(S2, U), U == qq(3, 4), op(0, xfy, [pp, qq])), true).
test(op_repeat, (op(333, xfy, [abc, abc, abc]), open_string('1 abc 2.', S), read(S, T), T == abc(1, 2), op(0, xfy, abc)), true).
test(op_drop_one, (op(200, fy, qq), op(500, yfx, qq), op(0, fy, qq), open_string('1 qq 2.', S), read(S, T), T == qq(1, 2), open_string('qq 1.', S2), read(S2, _)), error(syntax_error(_))).
test(op_clear_qq, op(0, yfx, qq), true).
test(op_drop_missing, op(0, fx, not_an_operator), true).
test(op_keep, (op(30, xfy, pp), catch((op(50, yf, pp), fail), error(permission_error(create, operator, pp), _), true), open_string('1 pp 2.', S), read(S, T), T == pp(1, 2), op(0, xfy, pp)), true).
test(op_list_keeps, (char_code(C, 44), catch(op(30, xfy, [pp, C]), error(permission_error(modify, operator, Culprit), _), Culprit == C), open_string('1 pp 2.', S), read(S, _)), error(syntax_error(_))).
test(op_pri_var, op(P, xfy, ++), error(instantiation_error)).
test(op_spec_var, op(30, S, ++), error(instantiation_error)).
test(op_name_var, op(30, xfy, N), error(instantiation_error)).
test(op_spec_before_type, op(max, S, ++), error(instantiation_error)).
test(op_partial, op(30, xfy, [pp|_]), error(instantiation_error)).
test(op_elem_var, op(30, xfy, [pp, E]), error(instantiation_error)).
test(op_type_int, op(max, xfy, ++), error(type_error(integer, max))).
test(op_type_float, op(1.5, xfy, pp), error(type_error(integer, 1.5))).
test(op_type_spec, op(30, 200, pp), error(type_error(atom, 200))).
test(op_type_spec_fun, op(30, f(1), pp), error(type_error(atom, f(1)))).
test(op_type_list, op(30, xfy, 0), error(type_error(list, 0))).
test(op_type_elem, op(30, xfy, [pp, 1]), error(type_error(atom, 1))).
test(op_type_before_spec, op(30, yfy, [1]), error(type_error(atom, 1))).
test(op_type_tail, op(30, xfy, [pp|1]), error(type_error(list, [pp|1]))).
test(op_type_pri_first, op(max, xfy, [pp|1]), error(type_error(integer, max))).
test(op_domain_neg, (N is -30, op(N, xfy, ++)), error(domain_error(operator_priority, -30))).
test(op_domain_hi, op(1201, xfy, ++), error(domain_error(operator_priority, 1201))).
test(op_domain_spec, op(30, yfy, ++), error(domain_error(operator_specifier, yfy))).
test(op_domain_zero_spec, op(0, yfy, ++), error(domain_error(operator_specifier, yfy))).
test(op_domain_before_comma, (char_code(C, 44), op(1201, xfy, C)), error(domain_error(operator_priority, 1201))).
test(op_spec_before_comma, (char_code(C, 44), op(30, yfy, C)), error(domain_error(operator_specifier, yfy))).
test(op_comma, (char_code(C, 44), catch(op(100, xfx, C), error(permission_error(modify, operator, Culprit), _), Culprit == C)), true).
test(op_comma_list, (char_code(C, 44), catch(op(100, xfx, [pp, C]), error(permission_error(modify, operator, Culprit), _), Culprit == C)), true).
test(op_comma_zero, (char_code(C, 44), catch(op(0, xfx, C), error(permission_error(modify, operator, Culprit), _), Culprit == C)), true).
test(op_empty, op(500, xfy, []), error(permission_error(create, operator, []))).
test(op_empty_list, op(500, xfy, [[]]), error(permission_error(create, operator, []))).
test(op_curly, op(500, xfy, {}), error(permission_error(create, operator, {}))).
test(op_curly_list, op(500, xfy, [{}]), error(permission_error(create, operator, {}))).
test(op_bar_low, op(1000, xfy, '|'), error(permission_error(create, operator, '|'))).
test(op_bar_list, op(1000, xfy, ['|']), error(permission_error(create, operator, '|'))).
test(op_bar_prefix, op(1150, fx, '|'), error(permission_error(create, operator, '|'))).
test(op_bar, (op(1105, xfy, '|'), open_string('a | b.', S), read(S, T), T == '|'(a, b), op(0, xfy, '|'), open_string('c | d.', S2), read(S2, _)), error(syntax_error(_))).

codes_op(Codes, Op) :- atom_codes(Op, Codes).

test(cop_neck_xfx, (codes_op([58, 45], Op), current_op(1200, xfx, Op)), true).
test(cop_arrow, (codes_op([45, 45, 62], Op), current_op(1200, xfx, Op)), true).
test(cop_neck_fx, (codes_op([58, 45], Op), current_op(1200, fx, Op)), true).
test(cop_query, (codes_op([63, 45], Op), current_op(1200, fx, Op)), true).
test(cop_semi, (codes_op([59], Op), current_op(1100, xfy, Op)), true).
test(cop_if, (codes_op([45, 62], Op), current_op(1050, xfy, Op)), true).
test(cop_comma, (codes_op([44], Op), current_op(1000, xfy, Op)), true).
test(cop_not, (codes_op([92, 43], Op), current_op(900, fy, Op)), true).
test(cop_unify, (codes_op([61], Op), current_op(700, xfx, Op)), true).
test(cop_not_unify, (codes_op([92, 61], Op), current_op(700, xfx, Op)), true).
test(cop_eq, (codes_op([61, 61], Op), current_op(700, xfx, Op)), true).
test(cop_neq, (codes_op([92, 61, 61], Op), current_op(700, xfx, Op)), true).
test(cop_term_lt, (codes_op([64, 60], Op), current_op(700, xfx, Op)), true).
test(cop_term_le, (codes_op([64, 61, 60], Op), current_op(700, xfx, Op)), true).
test(cop_term_gt, (codes_op([64, 62], Op), current_op(700, xfx, Op)), true).
test(cop_term_ge, (codes_op([64, 62, 61], Op), current_op(700, xfx, Op)), true).
test(cop_univ, (codes_op([61, 46, 46], Op), current_op(700, xfx, Op)), true).
test(cop_is, (codes_op([105, 115], Op), current_op(700, xfx, Op)), true).
test(cop_arith_eq, (codes_op([61, 58, 61], Op), current_op(700, xfx, Op)), true).
test(cop_arith_ne, (codes_op([61, 92, 61], Op), current_op(700, xfx, Op)), true).
test(cop_lt, (codes_op([60], Op), current_op(700, xfx, Op)), true).
test(cop_le, (codes_op([61, 60], Op), current_op(700, xfx, Op)), true).
test(cop_gt, (codes_op([62], Op), current_op(700, xfx, Op)), true).
test(cop_ge, (codes_op([62, 61], Op), current_op(700, xfx, Op)), true).
test(cop_plus_infix, (codes_op([43], Op), current_op(500, yfx, Op)), true).
test(cop_minus_infix, (codes_op([45], Op), current_op(500, yfx, Op)), true).
test(cop_bit_and, (codes_op([47, 92], Op), current_op(500, yfx, Op)), true).
test(cop_bit_or, (codes_op([92, 47], Op), current_op(500, yfx, Op)), true).
test(cop_star, (codes_op([42], Op), current_op(400, yfx, Op)), true).
test(cop_slash, (codes_op([47], Op), current_op(400, yfx, Op)), true).
test(cop_div_int, (codes_op([47, 47], Op), current_op(400, yfx, Op)), true).
test(cop_rem, (codes_op([114, 101, 109], Op), current_op(400, yfx, Op)), true).
test(cop_mod, (codes_op([109, 111, 100], Op), current_op(400, yfx, Op)), true).
test(cop_shift_l, (codes_op([60, 60], Op), current_op(400, yfx, Op)), true).
test(cop_shift_r, (codes_op([62, 62], Op), current_op(400, yfx, Op)), true).
test(cop_div, (codes_op([100, 105, 118], Op), current_op(400, yfx, Op)), true).
test(cop_pow, (codes_op([42, 42], Op), current_op(200, xfx, Op)), true).
test(cop_hat, (codes_op([94], Op), current_op(200, xfy, Op)), true).
test(cop_plus_prefix, (codes_op([43], Op), current_op(200, fy, Op)), true).
test(cop_minus_prefix, (codes_op([45], Op), current_op(200, fy, Op)), true).
test(cop_compl, (codes_op([92], Op), current_op(200, fy, Op)), true).
test(cop_plus_both, (codes_op([43], Op), findall(P-S, current_op(P, S, Op), L), L == [200-fy, 500-yfx]), true).
test(cop_minus_both, (codes_op([45], Op), findall(P-S, current_op(P, S, Op), L), L == [200-fy, 500-yfx]), true).
test(cop_neck_both, (codes_op([58, 45], Op), findall(P-S, current_op(P, S, Op), L), L == [1200-xfx, 1200-fx]), true).
test(cop_fy200, (findall(Op, current_op(200, fy, Op), L), codes_op([43], A), codes_op([45], B), codes_op([92], C), L == [A, B, C]), true).
test(cop_miss, (codes_op([43], Op), \+ current_op(100, xfx, Op)), true).
test(cop_bar_absent, (codes_op([124], Op), \+ current_op(_, _, Op)), true).
test(cop_user, (op(30, xfy, copp), current_op(30, xfy, copp), op(0, xfy, copp), \+ current_op(_, _, copp)), true).
test(cop_one_class, (op(30, xfx, copp), op(30, xfy, copp), findall(S, current_op(_, S, copp), L), L == [xfy], op(0, xfy, copp)), true).
test(cop_note, (op(444, fy, baz), op(444, fy, quux), findall(Op, (current_op(444, fy, Op), op(0, fy, quux)), L), L == [baz, quux], op(0, fy, baz)), true).
test(cop_note_clear, op(0, fy, [baz, quux]), true).
test(cop_zero, \+ current_op(0, _, _), true).
test(cop_zero_gone, (op(30, xfx, copp), op(0, xfx, copp), \+ current_op(0, _, copp)), true).
test(cop_pri_atom, current_op(a, _, _), error(domain_error(operator_priority, a))).
test(cop_pri_float, current_op(1.5, _, _), error(domain_error(operator_priority, 1.5))).
test(cop_pri_hi, current_op(1201, _, _), error(domain_error(operator_priority, 1201))).
test(cop_pri_neg, (N is -1, current_op(N, _, _)), error(domain_error(operator_priority, -1))).
test(cop_pri_before_spec, current_op(a, yfy, 1), error(domain_error(operator_priority, a))).
test(cop_spec_bad, current_op(_, yfy, _), error(domain_error(operator_specifier, yfy))).
test(cop_spec_int, current_op(_, 0, _), error(domain_error(operator_specifier, 0))).
test(cop_spec_fun, current_op(_, f(1), _), error(domain_error(operator_specifier, f(1)))).
test(cop_spec_before_name, current_op(500, yfy, 1), error(domain_error(operator_specifier, yfy))).
test(cop_name_int, current_op(_, _, 5), error(type_error(atom, 5))).
test(cop_name_fun, current_op(500, xfx, f(1)), error(type_error(atom, f(1)))).
test(cop_name_list, current_op(_, _, [a]), error(type_error(atom, [a]))).

o(1).
o(2).
o(3).

% 8.14.3.4。fx は o/1 の選択点より前なので 1 回。xf はその後ろなので、次の解では消えている。
% 前置と後置が両立すると write は前置を選ぶ。出力だけでは xf が残っても三つとも o N になる。
% with_output_to は失敗したゴールでは原子を束縛しない。失敗のあとに成功する選言を置く。
test(op_backtrack_prefix, (
  with_output_to(atom(A), (
    op(555, fx, o),
    o(X),
    (X = 1 -> true ; \+ current_op(555, xf, o)),
    write(o(X)), nl,
    op(555, xf, o),
    fail
    ; true
  )),
  A == 'o 1\no 2\no 3\n',
  op(0, fx, o),
  op(0, xf, o)
), true).

% 選択点が無い失敗は op/3 を残す。Cleanup の印は 0。
test(op_fail_keeps, (op(100, fx, zz), fail), fail).
test(op_fail_kept, current_op(100, fx, zz), true).
test(op_fail_clear, op(0, fx, zz), true).

% 成功した catch は枠を残すが armed を落とす。選択点が無ければ、中の op/3 は残す。
test(op_catch_keeps, (catch(op(100, fx, yy), _, true), fail), fail).
test(op_catch_kept, current_op(100, fx, yy), true).
test(op_catch_clear, op(0, fx, yy), true).

% 守っているゴールが失敗した catch は、その op/3 を戻す。
test(op_armed_catch_undo, catch((op(100, fx, aa), fail), _, true), fail).
test(op_armed_catch_gone, \+ current_op(100, fx, aa), true).

% throw を受けた catch も、中の op/3 を戻す。
test(op_throw_undo, (catch((op(100, fx, cc), throw(1)), _, true), \+ current_op(100, fx, cc)), true).

% ctest は build が作業ディレクトリ。直接実行はリポジトリの根。
consult_dd :- consult('tests/pl/op_keep_dir.pl').
consult_dd :- consult('../tests/pl/op_keep_dir.pl').

% op/3 の戻しは、その後の :- op を消さない。zz2 だけが選択点より後ろの op/3。
test(op_directive_survives, (
  (op(100, fx, zz2), consult_dd, fail ; current_op(200, xfx, dd), \+ current_op(100, fx, zz2)),
  op(0, xfx, dd),
  op(0, fx, zz2)
), true).
test(op_directive_clear, (op(0, xfx, dd), op(0, fx, zz2)), true).

