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
