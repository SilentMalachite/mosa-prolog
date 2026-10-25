:- dynamic(pet/1).
pet(cat).

:- dynamic(bug/1).
bug(ant).

:- dynamic(gone/1).
gone(a).
gone(b).

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

frozen(1).

test(assertz_pet, assertz(pet(dog)), true).
test(pet_both, pet(X), solutions([X = cat, X = dog])).
test(asserta_bug, asserta(bug(gnat)), true).
test(bug_order, bug(X), solutions([X = gnat, X = ant])).
test(assertz_static, assertz(frozen(2)), error(permission_error(modify, static_procedure, frozen/1))).
test(assertz_static_keeps, frozen(X), solutions([X = 1])).
test(asserta_static, asserta(frozen(0)), error(permission_error(modify, static_procedure, frozen/1))).
test(retract_one, retract(gone(a)), true).
test(gone_left, gone(X), solutions([X = b])).
test(retract_many, retract(many(X)), solutions([X = a, X = b])).
test(many_none, many(X), fail).
test(retract_static, retract(frozen(1)), error(permission_error(modify, static_procedure, frozen/1))).
test(retractall_ok, retractall(allgone(_)), true).
test(allgone_none, allgone(X), fail).
test(retractall_static, retractall(frozen(_)), error(permission_error(modify, static_procedure, frozen/1))).
test(abolish_ok, abolish(temp/1), true).
test(temp_gone, temp(X), error(existence_error(procedure, temp/1))).
test(abolish_static, abolish(frozen/1), error(permission_error(modify, static_procedure, frozen/1))).
test(frozen_remains, frozen(X), solutions([X = 1])).
test(asserta_once, (step(X), X = 1, asserta(step(0))), solutions([X = 1])).
test(step_now, step(X), solutions([X = 0, X = 1, X = 2])).
