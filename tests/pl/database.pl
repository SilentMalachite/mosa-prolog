:- dynamic(pet/1).
pet(cat).

:- dynamic(bug/1).
bug(ant).

:- dynamic(gone/1).
gone(a).

:- dynamic(many/1).
many(a).
many(b).

:- dynamic(allgone/1).
allgone(a).
allgone(b).

:- dynamic(temp/1).
temp(a).

:- dynamic(step/1).
step(1).
step(2).

:- dynamic(seen/1).
seen(1).
seen(2).

:- dynamic(going/1).
going(X) :- retract((going(X) :- _)), still(X).
still(1).

:- dynamic(mix/1).
mix(a).
mix(X) :- mix_body(X).
mix_body(b).

:- dynamic(allrule/1).
allrule(a).
allrule(X) :- allrule_body(X).
allrule_body(b).

:- dynamic(keep/1).
keep(a).
keep(b).

:- dynamic(none/1).

:- dynamic(box/1).

frozen(1).

test(assertz_pet, assertz(pet(dog)), true).
test(pet_both, pet(X), solutions([X = cat, X = dog])).
test(asserta_bug, asserta(bug(gnat)), true).
test(bug_order, bug(X), solutions([X = gnat, X = ant])).
test(assertz_static, assertz(frozen(2)), error(permission_error(modify, static_procedure, frozen/1))).
test(assertz_static_keeps, frozen(X), solutions([X = 1])).
test(asserta_static, asserta(frozen(0)), error(permission_error(modify, static_procedure, frozen/1))).
test(retract_one, retract(gone(a)), true).
test(gone_left, gone(X), fail).
test(retract_many, retract(many(X)), solutions([X = a, X = b])).
test(many_none, many(X), fail).
test(retract_static, retract(frozen(1)), error(permission_error(modify, static_procedure, frozen/1))).
test(retractall_ok, retractall(allgone(_)), true).
test(allgone_none, allgone(X), fail).

:- dynamic(wipe_aa/1).
wipe_aa(1).
wipe_aa(2).
wipe_aa(3).
wipe_aa_all :- retractall(wipe_aa(_)).

test(retractall_in_clause, wipe_aa_all, true).
test(retractall_in_clause_gone, wipe_aa(X), fail).
test(retractall_static, retractall(frozen(_)), error(permission_error(modify, static_procedure, frozen/1))).
test(abolish_ok, abolish(temp/1), true).
test(temp_gone, temp(X), error(existence_error(procedure, temp/1))).
test(assertz_after_abolish, assertz(temp(b)), true).
test(temp_back, temp(X), solutions([X = b])).
test(abolish_static, abolish(frozen/1), error(permission_error(modify, static_procedure, frozen/1))).
test(frozen_remains, frozen(X), solutions([X = 1])).
test(open_misses_asserta, (step(X), (X = 1 -> asserta(step(0)) ; true)), solutions([X = 1, X = 2])).
test(step_now, step(X), solutions([X = 0, X = 1, X = 2])).
test(open_misses_assertz, (seen(X), assertz(seen(9))), solutions([X = 1, X = 2])).
test(seen_later, seen(X), solutions([X = 1, X = 2, X = 9, X = 9])).
test(retract_keeps_body, going(X), solutions([X = 1])).
test(going_gone, going(X), fail).
test(retract_sticks, (retract(keep(a)), fail), fail).
test(keep_left, keep(X), solutions([X = b])).
test(retractall_empty, retractall(none(_)), true).
test(assert_copies, (assertz(box(Z)), Z = 1, box(Y), Y == 1), fail).
test(assertz_var, assertz(X), error(instantiation_error)).
test(assertz_number, assertz(1), error(type_error(callable, 1))).
test(assertz_head_var, assertz((X :- true)), error(instantiation_error)).
test(assertz_body_number, assertz((ok :- 1)), error(type_error(callable, 1))).

:- dynamic(later/1).
later(1).
later(2).
later(3).

:- dynamic(scan_aa/1).
scan_aa(1).
scan_aa(2).
scan_aa(3).

test(logical_update_asserta, (with_output_to(atom(A), ((scan_aa(X), write(X), asserta(scan_aa(4)), asserta(scan_aa(5)), fail) ; true)), A == '123'), true).

test(retract_open_later, (later(X), (X = 1 -> retract(later(3)) ; true)), solutions([X = 1, X = 2])).
test(later_left, later(X), solutions([X = 1, X = 2])).
test(assertz_builtin, assertz(true), error(permission_error(modify, static_procedure, true/0))).
test(retract_builtin, retract(fail), error(permission_error(modify, static_procedure, fail/0))).
test(abolish_builtin, abolish(atom/1), error(permission_error(modify, static_procedure, atom/1))).
test(assertz_builtin_is, assertz(is(1, 1)), error(permission_error(modify, static_procedure, is/2))).
test(retract_fact_only, (retract(mix(Y)), \+ retract(mix(_))), solutions([Y = a])).
test(mix_rule_left, mix(Y), solutions([Y = b])).
test(retractall_rules, retractall(allrule(_)), true).
test(allrule_none, allrule(X), fail).
test(assertz_throw, assertz(throw(1)), error(permission_error(modify, static_procedure, throw/1))).
test(abolish_catch, abolish(catch/3), error(permission_error(modify, static_procedure, catch/3))).
test(abolish_comma, abolish((',')/2), error(permission_error(modify, static_procedure, (',')/2))).
test(assertz_arrow, assertz((a -> b)), error(permission_error(modify, static_procedure, (->)/2))).
% (:-)/1 は指示、(:-)/2 は節の首。組込表に無くても静的なので asserta は足せない。
test(asserta_neck_2, asserta(((a :- b) :- true)), error(permission_error(modify, static_procedure, (:-)/2))).
test(asserta_neck_1, asserta(((:- b) :- true)), error(permission_error(modify, static_procedure, (:-)/1))).
test(assert_nested, assertz((nest_phase3 :- (true, 4))), error(type_error(callable, (true, 4)))).
test(assert_builtin_body, assertz((atom(_) :- (true, 4))), error(type_error(callable, (true, 4)))).
