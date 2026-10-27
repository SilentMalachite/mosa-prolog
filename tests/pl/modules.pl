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

use_expose :- use_module('tests/pl/modules/expose.pl').
use_expose :- use_module('../tests/pl/modules/expose.pl').

% 0 は呼び出し元で包む。? は包まない。
test(mod_meta_wrap, (use_expose, show(seen_meta(1), user:seen_meta(1))), true).
meta_seen(1).
% 包んだゴールは定義したモジュールではなく、呼んだ側で実行する。
test(mod_meta_run, (use_expose, run(meta_seen(1))), true).
% すでに Module:Goal なら、呼んだ側でもう一度包まない。
test(mod_meta_kept, (use_expose, show(greet:secret(1), greet:secret(1))), true).
% 数字は追加する引数の個数。call/N は限定の内側へ足す。
test(mod_meta_extra, (use_expose, call_one(meta_seen)), true).
% ^ は Vars^Goal の Goal だけを包む。^ でない項は全体を包む。
test(mod_meta_caret, (use_expose, take(X^meta_seen(X), X^(user:meta_seen(X)))), true).
test(mod_meta_caret_whole, (use_expose, take(plain_meta(1), user:plain_meta(1))), true).
% // は 名前/アリティ を呼び出し元:名前/アリティ にする。
test(mod_meta_slash, (use_expose, pi(meta_seen/1, user:meta_seen/1)), true).
% 一つの指示にコンマで並べた仕様は、どちらも包む。
test(mod_meta_comma, (use_expose, left_goal(meta_seen(1)), right_goal(meta_seen(1))), true).
% : も 0 と同じく呼び出し元で包む。
test(mod_meta_colon, (use_expose, colon_run(meta_seen(1))), true).
% モジュールの中の findall は、そのモジュールの述語を集める。
test(mod_meta_findall, (use_expose, expose_items(Xs), Xs == [a]), true).
% すでに限定したゴールを call/1 に渡しても、呼んだ側で包み直さない。
test(mod_meta_call_qual, call(greet:secret(1)), true).

% 修飾した assertz は名付けたモジュールへ書く。user には席を作らない。
test(mod_assert_qualified, (assertz(kin:spare(1)), kin:spare(1), \+ spare(1)), true).
% 節の頭部に付いた修飾も、同じモジュールの規則になる。
test(mod_assert_rule, (assertz((kin:ruled(X) :- X = 1)), kin:ruled(1)), true).
% 修飾した retract は、そのモジュールの節を消し、user の同名述語にはしない。
test(mod_retract_qualified, (assertz(kin:gone(1)), retract(kin:gone(1)), \+ kin:gone(1)), true).
% 修飾した retractall は、そのモジュールの合う節をすべて消す。
test(mod_retractall_qualified, (
  assertz(kin:bulk(1)), assertz(kin:bulk(2)), retractall(kin:bulk(_)), \+ kin:bulk(_)
), true).
% 修飾した abolish は、名付けたモジュールの述語を消す。
test(mod_abolish_qualified, (
  assertz(kin:drop(1)), abolish(kin:drop/1), \+ clause(kin:drop(_), _)
), true).
% 載っていないモジュールへの assertz は、:/2 の節にはしない。
test(mod_assert_no_module, assertz(no_such_mod:spare(1)), error(existence_error(module, no_such_mod))).
% dynamic(Module:PI) は、そのモジュールに節の無い動的述語を作る。
test(mod_dynamic_qualified, (dynamic(kin:later/1), \+ kin:later(_)), true).
% 取り込みだけの枠を dynamic にすると、席が動的になり公開した側へ書けてしまう。
test(mod_dynamic_import, dynamic(parent/2), error(permission_error(modify, static_procedure, parent/2))).
% モジュールが変数なら、型より先に具体化エラー。:/2 の節にはしない。
test(mod_assert_var_module, assertz(M:spare(1)), error(instantiation_error)).
% モジュールがアトムでなければ型エラー。
test(mod_assert_type_module, assertz(1:spare(1)), error(type_error(atom, 1))).
% 取り込みだけの assert は公開した側の節を増やさない。
test(mod_assert_import, assertz(parent(z, z)), error(permission_error(modify, static_procedure, parent/2))).
test(mod_assert_import_keeps, (kin:parent(a, b), \+ kin:parent(z, z)), true).
% 節の本体で足す節は、その節を定義したモジュールに入る。
test(mod_assert_home, (
  assertz((kin:add_note :- assertz(noted(9)))), kin:add_note, kin:noted(9), \+ noted(9)
), true).
% dynamic/1 は組込なので、同名の節は足せない。
test(mod_assert_dynamic, assertz(dynamic(a)), error(permission_error(modify, static_procedure, dynamic/1))).

consult_dynbox :- consult('tests/pl/modules/dynbox.pl').
consult_dynbox :- consult('../tests/pl/modules/dynbox.pl').

% ファイル中の dynamic(Module:PI) は、読み込み中のモジュールではなく名付けた側に空の述語を作る。
test(mod_dynamic_directive, (consult_dynbox, current_predicate(boxnote/1), \+ boxnote(_)), true).

consult_mf_own :- consult('tests/pl/modules/mf_own.pl').
consult_mf_own :- consult('../tests/pl/modules/mf_own.pl').

consult_mf_add :- consult('tests/pl/modules/mf_add.pl').
consult_mf_add :- consult('../tests/pl/modules/mf_add.pl').

% 足す側は自分のモジュールを持ち、multifile(Owner:PI) のあと Owner:Head を書く。
test(mod_multifile_add, (
  consult_mf_own, consult_mf_add, findall(X, mf_own:shared(X), L), L == [1, 2]
), true).

consult_mf_bare :- consult('tests/pl/modules/mf_bare.pl').
consult_mf_bare :- consult('../tests/pl/modules/mf_bare.pl').

% 足す側の宣言が無い修飾した節は、読み込みを許可エラーで失敗させる。持つ側の節は残る。
test(mod_multifile_foreign, consult_mf_bare, error(permission_error(modify, static_procedure, shared/1))).
test(mod_multifile_foreign_keeps, (findall(X, mf_own:shared(X), L), L == [1, 2]), true).

consult_mf_rule_own :- consult('tests/pl/modules/mf_rule_own.pl').
consult_mf_rule_own :- consult('../tests/pl/modules/mf_rule_own.pl').

consult_mf_rule :- consult('tests/pl/modules/mf_rule.pl').
consult_mf_rule :- consult('../tests/pl/modules/mf_rule.pl').

% (Owner:Head :- Body) は、Head の節を Owner に足す。
test(mod_multifile_rule, (
  consult_mf_rule_own, consult_mf_rule, findall(X, mf_rule_own:ruled(X), L), L == [0, 1]
), true).

consult_mf_nodec :- consult('tests/pl/modules/mf_nodec.pl').
consult_mf_nodec :- consult('../tests/pl/modules/mf_nodec.pl').

% 持つ側が宣言していない述語へは、足す側の宣言だけでは節を書けない。
test(mod_multifile_owner, consult_mf_nodec, error(permission_error(modify, static_procedure, plain/1))).
test(mod_multifile_owner_absent, (
  catch(mf_own:plain(_), error(existence_error(procedure, plain/1), _), true)
), true).

consult_mf_local :- consult('tests/pl/modules/mf_local.pl').
consult_mf_local :- consult('../tests/pl/modules/mf_local.pl').

% 裸の節は、足す側のファイル自身のモジュールに属する。
test(mod_multifile_local, (
  consult_mf_local,
  findall(X, mf_own:shared(X), L), L == [1, 2, 4],
  mf_local:mine(7),
  \+ mf_own:mine(_)
), true).
% 取り込みだけの枠は節を持たない。clause は公開した側を読まず、改変の許可エラーにする。
test(mod_clause_import, clause(parent(a, _), _), error(permission_error(modify, static_procedure, parent/2))).
% 修飾した clause は、名付けたモジュールの節を読む。
test(mod_clause_qualified, (clause(kin:parent(a, X), true), X == b), true).

consult_op_box :- consult('tests/pl/modules/op_box.pl').
consult_op_box :- consult('../tests/pl/modules/op_box.pl').

% op/3 は実行したモジュールの表だけを変える。user の current_op には出ない。
test(mod_op_local, (
  consult_op_box,
  op_box:op(400, xfx, ownop),
  \+ current_op(400, xfx, ownop),
  op_box:current_op(400, xfx, ownop),
  op(0, xfx, ownop)
), true).

% モジュールから見ても、システムの演算子は残る。op_box は前のテストが読んでいる。
test(mod_op_sees_system, op_box:current_op(700, xfx, (=)), true).

% user で宣言した演算子は、どのモジュールの current_op からも見える。
test(mod_op_sees_user, (
  op(333, xfx, userop),
  op_box:current_op(333, xfx, userop),
  op(0, xfx, userop)
), true).

consult_op_user :- consult('tests/pl/modules/op_user.pl').
consult_op_user :- consult('../tests/pl/modules/op_user.pl').

% ファイルを読んでいるあいだ、user の演算子はモジュールの項にも効く。
test(mod_op_user_read, (
  op(333, xfx, userop),
  consult_op_user,
  op_user:seen(userop(a, b)),
  op(0, xfx, userop)
), true).

consult_op_file :- consult('tests/pl/modules/op_file.pl').
consult_op_file :- consult('../tests/pl/modules/op_file.pl').

% ファイルの :- op は、その場の項には効く。読み終わると user の表には残らない。
test(mod_op_file, (
  consult_op_file,
  op_file:seen(fileop(a, b)),
  \+ current_op(400, xfx, fileop)
), true).

% 同じ宣言は、宣言したモジュールの current_op には残る。
test(mod_op_file_keeps, op_file:current_op(400, xfx, fileop), true).

use_op_exp :- use_module('tests/pl/modules/op_exp.pl').
use_op_exp :- use_module('../tests/pl/modules/op_exp.pl').

% 公開リストの op/3 は、取り込んだモジュールの表に入る。
test(mod_op_export, (
  use_op_exp,
  current_op(400, xfx, expop),
  open_string('a expop b.', S),
  read(S, T),
  T == expop(a, b),
  op(0, xfx, expop)
), true).

consult_op_imp :- consult('tests/pl/modules/op_imp.pl').
consult_op_imp :- consult('../tests/pl/modules/op_imp.pl').

% 読み込み中のモジュールが公開演算子を取り込むと、続きの項とその current_op に効く。
test(mod_op_imported, (
  consult_op_imp,
  op_imp:seen(expop(a, b)),
  \+ current_op(400, xfx, expop),
  op_imp:current_op(400, xfx, expop)
), true).

% except は述語を除いても、公開した演算子は取り込む。
test(mod_op_except, (
  use_module(op_exp, except([missing/1])),
  current_op(400, xfx, expop),
  op(0, xfx, expop)
), true).

consult_op_bad :- consult('tests/pl/modules/op_bad.pl').
consult_op_bad :- consult('../tests/pl/modules/op_bad.pl').

consult_op_retry :- consult('tests/pl/modules/op_retry.pl').
consult_op_retry :- consult('../tests/pl/modules/op_retry.pl').

% 失敗した読み込みの演算子は、同じ名前を読み直しても残らない。
test(mod_op_rollback, (
  \+ consult_op_bad,
  consult_op_retry,
  \+ op_bad:current_op(400, xfx, badop)
), true).

consult_flag_dq :- consult('tests/pl/modules/flag_dq.pl').
consult_flag_dq :- consult('../tests/pl/modules/flag_dq.pl').

% ファイルが変えた double_quotes は、読み終わると戻る。モジュールは旗を持たない。
test(mod_flag_quotes_restored, (
  consult_flag_dq,
  current_prolog_flag(double_quotes, codes),
  flag_dq:current_prolog_flag(double_quotes, codes)
), true).

consult_flag_read :- consult('tests/pl/modules/flag_read.pl').
consult_flag_read :- consult('../tests/pl/modules/flag_read.pl').

% 指示のあとの項は、そのファイルの double_quotes で読む。読み終わると戻る。
test(mod_flag_quotes_reads, (
  consult_flag_read,
  flag_read:seen(ab),
  current_prolog_flag(double_quotes, codes)
), true).

consult_flag_from :- consult('tests/pl/modules/flag_from.pl').
consult_flag_from :- consult('../tests/pl/modules/flag_from.pl').

% 戻す値は既定の codes ではなく、読み始める前の値。
test(mod_flag_quotes_saved, (
  set_prolog_flag(double_quotes, chars),
  consult_flag_from,
  current_prolog_flag(double_quotes, chars),
  set_prolog_flag(double_quotes, codes)
), true).

consult_flag_bad :- consult('tests/pl/modules/flag_bad.pl').
consult_flag_bad :- consult('../tests/pl/modules/flag_bad.pl').

% 失敗した読み込みも double_quotes を戻す。
test(mod_flag_quotes_failed, (
  \+ consult_flag_bad,
  current_prolog_flag(double_quotes, codes)
), true).

consult_flag_ja :- consult('tests/pl/modules/flag_ja.pl').
consult_flag_ja :- consult('../tests/pl/modules/flag_ja.pl').

% ファイルが変えた japanese_punctuation は、読み終わると戻る。モジュールは旗を持たない。
test(mod_flag_ja_restored, (
  consult_flag_ja,
  current_prolog_flag(japanese_punctuation, false),
  flag_ja:current_prolog_flag(japanese_punctuation, false)
), true).

consult_flag_outer :- consult('tests/pl/modules/flag_outer.pl').
consult_flag_outer :- consult('../tests/pl/modules/flag_outer.pl').

% 入れ子の読み込みは、自分の double_quotes だけを戻す。親の続きはその親の値で読む。
test(mod_flag_quotes_nested, (
  consult_flag_outer,
  flag_inner:seen(ab),
  flag_outer:seen([c, d]),
  current_prolog_flag(double_quotes, codes),
  flag_outer:current_prolog_flag(double_quotes, codes)
), true).

consult_flag_ja_bad :- consult('tests/pl/modules/flag_ja_bad.pl').
consult_flag_ja_bad :- consult('../tests/pl/modules/flag_ja_bad.pl').

% 失敗した読み込みも japanese_punctuation を戻す。
test(mod_flag_ja_failed, (
  \+ consult_flag_ja_bad,
  current_prolog_flag(japanese_punctuation, false)
), true).

consult_flag_ja_from :- consult('tests/pl/modules/flag_ja_from.pl').
consult_flag_ja_from :- consult('../tests/pl/modules/flag_ja_from.pl').

% 戻す値は既定の false ではなく、読み始める前の値。
test(mod_flag_ja_saved, (
  set_prolog_flag(japanese_punctuation, true),
  consult_flag_ja_from,
  current_prolog_flag(japanese_punctuation, true),
  set_prolog_flag(japanese_punctuation, false)
), true).

consult_flag_ja_lex :- consult('tests/pl/modules/flag_ja_lex.pl').
consult_flag_ja_lex :- consult('../tests/pl/modules/flag_ja_lex.pl').

% true でも全角括弧は括弧にならない。失敗した読み込みのあと、旗は戻る。
test(mod_flag_ja_lex, (
  \+ consult_flag_ja_lex,
  current_prolog_flag(japanese_punctuation, false)
), true).

consult_flag_inc :- consult('tests/pl/modules/flag_inc.pl').
consult_flag_inc :- consult('../tests/pl/modules/flag_inc.pl').

% include が変えた double_quotes は、そのファイルの終わりで戻る。親の初期化は親の値を見る。
test(mod_flag_include, (
  consult_flag_inc,
  current_prolog_flag(double_quotes, codes)
), true).
