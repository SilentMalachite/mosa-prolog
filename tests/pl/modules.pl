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

consult_late :- consult('tests/pl/modules/late.pl').
consult_late :- consult('../tests/pl/modules/late.pl').

test(mod_not_first, consult_late, error(permission_error(modify, module, late))).
test(mod_not_first_gone, (
  catch(consult_late, error(permission_error(modify, module, late), _), true),
  \+ current_module(late),
  \+ current_predicate(plain/1)
), true).

consult_twice :- consult('tests/pl/modules/twice.pl').
consult_twice :- consult('../tests/pl/modules/twice.pl').

test(mod_second, consult_twice, error(permission_error(modify, module, again))).
test(mod_second_gone, (
  catch(consult_twice, error(permission_error(modify, module, again), _), true),
  \+ current_module(twice),
  \+ current_module(again)
), true).

consult_kin_again :- consult('tests/pl/modules/kin_again.pl').
consult_kin_again :- consult('../tests/pl/modules/kin_again.pl').

test(mod_other_file, consult_kin_again, error(permission_error(modify, module, kin))).
test(mod_other_file_kept, (
  catch(consult_kin_again, error(permission_error(modify, module, kin), _), true),
  current_module(kin),
  \+ current_predicate(intruder/1)
), true).

use_greet :- use_module('tests/pl/modules/greet.pl').
use_greet :- use_module('../tests/pl/modules/greet.pl').

test(mod_use, (use_greet, hello(world)), true).
% mod_use が greet を載せた。公開していない述語は、ここからは見えない。
test(mod_use_hides, \+ secret(1), true).

use_kin :- use_module('tests/pl/modules/kin.pl').
use_kin :- use_module('../tests/pl/modules/kin.pl').

test(mod_use_loaded, (use_kin, parent(a, b)), true).

use_pair_one :- use_module('tests/pl/modules/pair.pl', [one/1]).
use_pair_one :- use_module('../tests/pl/modules/pair.pl', [one/1]).

% リストに書いた公開述語だけを取り込む。two/1 は公開されているが、ここからは見えない。
test(mod_use_list, (use_pair_one, one(1), \+ two(1)), true).

use_alias :- use_module('tests/pl/modules/alias.pl', [source_name/1 as local_name]).
use_alias :- use_module('../tests/pl/modules/alias.pl', [source_name/1 as local_name]).

% 別名で取り込み、元の名前は呼び出し側に置かない。as は大域の演算子ではない。
test(mod_use_alias, (use_alias, local_name(ok), \+ source_name(ok)), true).
test(mod_as_not_op, (open_string('a as b.', S), read(S, _)), error(syntax_error(_))).
