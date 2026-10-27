test(mod_user, current_module(user), true).
test(mod_system, current_module(system), true).
test(mod_absent, current_module(no_such_module), fail).
test(mod_type, current_module(1), error(type_error(atom, 1))).
test(mod_type_fun, current_module(f(1)), error(type_error(atom, f(1)))).
test(mod_gen, current_module(M), solutions([M = user, M = system])).

% ctest は build が作業ディレクトリ。直接実行はリポジトリの根。
consult_kin :- consult('tests/pl/modules/kin.pl').
consult_kin :- consult('../tests/pl/modules/kin.pl').

test(mod_consult_seen, (consult_kin, current_module(kin)), true).
% 直前の consult が kin を載せた。選択点の中で同じファイルを読み直すと、今の consult は失敗する。
test(mod_consult_hides, \+ parent(a, b), true).
