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
going(X) :- retract(going(X)), still(X).
still(1).

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

test(retract_open_later, (later(X), (X = 1 -> retract(later(3)) ; true)), solutions([X = 1, X = 2])).
test(later_left, later(X), solutions([X = 1, X = 2])).
test(assertz_builtin, assertz(true), error(permission_error(modify, static_procedure, true/0))).
test(retract_builtin, retract(fail), error(permission_error(modify, static_procedure, fail/0))).
test(abolish_builtin, abolish(atom/1), error(permission_error(modify, static_procedure, atom/1))).
test(assertz_builtin_is, assertz(is(1, 1)), error(permission_error(modify, static_procedure, is/2))).
