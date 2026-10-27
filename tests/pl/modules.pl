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

use_shade :- use_module('tests/pl/modules/shade.pl', except([dropped/1])).
use_shade :- use_module('../tests/pl/modules/shade.pl', except([dropped/1])).

% except は書いた公開述語を外し、残りの公開述語を取り込む。
test(mod_use_except, (use_shade, kept(1), \+ dropped(1)), true).

use_left :- use_module('tests/pl/modules/left.pl').
use_left :- use_module('../tests/pl/modules/left.pl').

use_right :- use_module('tests/pl/modules/right.pl').
use_right :- use_module('../tests/pl/modules/right.pl').

test(mod_import_clash, (use_left, use_right), error(permission_error(import_into, procedure, same/1))).
% 衝突しても先の取り込みは残り、読み込んだモジュールも残る。新しい席は増えない。
test(mod_import_clash_kept, (
  catch(use_right, error(permission_error(import_into, procedure, same/1), _), true),
  current_module(left_side),
  current_module(right_side),
  same(left),
  \+ same(right)
), true).

use_shelf :- use_module(library(phase4_shelf)).

% library(Name) は実行ファイルの隣の lib/Name.pl を読み、公開述語を取り込む。
test(mod_library, (use_shelf, shelf_mark(1)), true).
% 無いライブラリは、無いファイルを consult したときと同じくゴールが失敗する。
test(mod_library_missing, use_module(library(phase4_no_such_lib)), fail).

use_lists :- use_module(library(lists)).

% library(lists) を取り込むと member/2 が成功する。
test(mod_lists_member, (use_lists, member(花子, [次郎, 花子])), true).

use_lists_length :- use_module(library(lists), [length/2]).

% length/2 は system にある。library(lists) が再公開するので、名前を指定して取り込める。
test(mod_lists_length, (use_lists_length, current_predicate(length/2), length([a, b], 2)), true).

% 公開リストに無い述語も、Module:Goal はそのモジュールで実行する。
test(mod_qualify_hidden, greet:secret(1), true).
% 成功したあとは呼び出し元に戻る。裸の名前では、公開していない述語は見えない。
test(mod_qualify_restores, (greet:secret(1), \+ secret(1)), true).
% 失敗したときも呼び出し元に戻る。戻らないと secret/1 が greet のまま成功する。
test(mod_qualify_fail_restores, (\+ greet:secret(2), \+ secret(1)), true).
% ロードしていないモジュールは手続きの存在エラーではなく、モジュールの存在エラー。
test(mod_qualify_unknown, phase4_no_such:true, error(existence_error(module, phase4_no_such))).
% モジュールが変数なら、型を見る前に instantiation_error。
test(mod_qualify_var, _:true, error(instantiation_error)).
% アトム以外は type_error。変数は上で先に弾く。
test(mod_qualify_type, 1:true, error(type_error(atom, 1))).
% M1:M2:Goal の呼び出し元は M2。user には secret/1 が無い。
test(mod_qualify_inner, user:greet:secret(1), true).
test(mod_qualify_outer, \+ greet:user:secret(1), true).
% モジュールが正しくても、ゴールが callable でなければ type_error。
test(mod_qualify_goal, greet:1, error(type_error(callable, 1))).
% 例外で抜けても呼び出し元に戻る。戻らないと回復ゴールが greet の secret/1 を見る。
test(mod_qualify_throw, (catch(greet:throw(ball), ball, \+ secret(1))), true).
% user と system もロード済みモジュールである。
test(mod_qualify_user, user:hello(world), true).
test(mod_qualify_system, system:true, true).
% 複合項のモジュールもアトムではない。
test(mod_qualify_type_fun, f(1):true, error(type_error(atom, f(1)))).
% ゴールが変数なら instantiation_error。モジュールの検査は先に通る。
test(mod_qualify_goal_var, greet:G, error(instantiation_error)).
% 中のカットは外側の選言を消さない。
test(mod_qualify_cut, (greet:!, fail ; true), true).
% 節が複数あっても、尽きたあとは呼び出し元に戻る。
test(mod_qualify_redo, (
  findall(X, lists:member(X, [a, b]), Xs),
  Xs == [a, b],
  hello(world)
), true).
