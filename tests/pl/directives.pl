% 7.4.2 discontiguous/1。指示だけ。同じテキストで、隙間より前に宣言した述語は節を離してよい。
% 宣言が無い述語は、診断「述語 名前/アリティ の節が離れている」のまま失敗する。
% 宣言は手続きを定義しない。次の consult は、multifile が無いので置き換える。

:- discontiguous(dc_p/1).

dc_p(1).
dc_q(1).
dc_p(2).

:- discontiguous([dc_a/1, dc_b/0]).

dc_a(1).
dc_mid(1).
dc_b.
dc_a(2).
dc_late_gap(1).
dc_b.

:- discontiguous(dc_rule/1).

dc_rule(1).
dc_side(9).
dc_rule(X) :- dc_side(X).

:- discontiguous(dc_only/1).

dc_after(1).

:- discontiguous(dc_after/1).

dc_between(1).
dc_after(2).

test(dc_order, (findall(X, dc_p(X), L), L == [1, 2]), true).
test(dc_neighbor, dc_q(1), true).
test(dc_list_a, (findall(X, dc_a(X), L), L == [1, 2]), true).
test(dc_list_b, (findall(t, dc_b, L), L == [t, t]), true).
test(dc_rule, (findall(X, dc_rule(X), L), L == [1, 9]), true).
test(dc_only_absent, current_predicate(dc_only/1), fail).
% iso-notes 7.8.3 の決定 B。宣言だけの述語は呼べば失敗する。current_predicate/1 には出ない。
test(dc_only_call, dc_only(_), fail).
test(dc_only_call1, call(dc_only(_)), fail).
test(dc_after_first, (findall(X, dc_after(X), L), L == [1, 2]), true).

% syntax/directives/discontiguous_1 と同じ、引数 2〜4 の述語が互いに分断する形。宣言はリストで書く。
:- discontiguous([dc_c/2, dc_d/3, dc_e/4]).

dc_c(1, 2).
dc_d(1, 2, 3).
dc_c(3, 4).
dc_e(1, 2, 3, 4).
dc_d(4, 5, 6).
dc_e(5, 6, 7, 8).

test(dc_arity2, (findall(X-Y, dc_c(X, Y), L), L == [1-2, 3-4]), true).
test(dc_arity3, (findall(X-Y-Z, dc_d(X, Y, Z), L), L == [1-2-3, 4-5-6]), true).
test(dc_arity4, (findall(X-Y-Z-W, dc_e(X, Y, Z, W), L), L == [1-2-3-4, 5-6-7-8]), true).

% iso-notes 7.4.2 discontiguous/1 の決定 B（2026-10-28）。3 つの指示はカンマの並びも受け付ける。
:- discontiguous((dc_s/1, dc_t/2)).

dc_s(1).
dc_t(1, 2).
dc_s(2).
dc_t(3, 4).

:- dynamic((dyn_s/1, dyn_t/2)).
:- multifile((mf_s/1, mf_t/0)).

mf_s(1).
mf_t.

test(dc_seq_first, (findall(X, dc_s(X), L), L == [1, 2]), true).
test(dc_seq_second, (findall(X-Y, dc_t(X, Y), L), L == [1-2, 3-4]), true).
test(dyn_seq_first, dyn_s(_), fail).
test(dyn_seq_second, dyn_t(_, _), fail).
test(mf_seq, (mf_s(1), mf_t), true).

% 7.4.2 multifile/1。指示だけ。引数は述語指示子かそのリスト。
% 宣言は手続きを定義しない。宣言した述語は次の読み込みが節を足す。
% 宣言が無い述語は置き換わる。ここでは同じテキストの節だけを見る。

:- multifile(mf_p/1).

mf_p(1).
mf_p(2).

:- multifile([mf_a/1, mf_b/0]).

mf_a(1).
mf_a(2).
mf_b.

:- multifile(mf_rule/1).

mf_rule(1).
mf_rule(X) :- mf_side(X).
mf_side(9).

:- multifile(mf_only/1).
:- multifile(mf_hook/1).

test(mf_same, (findall(X, mf_p(X), L), L == [1, 2]), true).
test(mf_list_a, (findall(X, mf_a(X), L), L == [1, 2]), true).
test(mf_list_b, (findall(t, mf_b, L), L == [t]), true).
test(mf_rule, (findall(X, mf_rule(X), L), L == [1, 9]), true).
test(mf_only_absent, current_predicate(mf_only/1), fail).
test(mf_only_call, mf_only(_), fail).
test(mf_only_call1, call(mf_only(_)), fail).
test(mf_hook_assert, (assertz(mf_hook(1)), mf_hook(X)), solutions([X = 1])).
test(mf_static, assertz(mf_p(3)), error(permission_error(modify, static_procedure, mf_p/1))).

% 7.4.2 initialization/1。指示だけ。ゴールはテキストを準備したあとに、ソースの順で一度実行する。
% 失敗はエラーにしない。捕捉されない例外は読み込みを失敗させ、節は残す。

:- dynamic(init_s/1).
:- initialization((retractall(init_s(_)), assertz(init_s(1)))).

test(init_ran, (findall(X, init_s(X), L), L == [1]), true).
test(init_goal, initialization(true), error(existence_error(procedure, initialization/1))).

% 7.4.2 に無い指示 :- G（iso-notes 7.4.2 の一般の指示）。その位置で一度呼ぶので、後の指示は前の指示の効果を見る。
% gd_base/1 は指示より上の節で、指示はそれを呼べる。retractall をしないので、2 回動けば L は 4 要素になる。
% 失敗と例外が読み込みを戻すことは tests/cpp/machine_test.cpp の check_general_directive。

:- dynamic(gd_seen/1).
gd_base(first).
:- gd_base(X), assertz(gd_seen(X)).
:- (gd_seen(first) -> assertz(gd_seen(second)) ; true).

test(general_directive_order, (findall(X, gd_seen(X), L), L == [first, second]), true).

% 7.4.2 include/1。指示だけ。節は読み込み中のテキストの途中に入る。
% ファイルが要る確認は tests/cpp/include_test.cpp。ここはゴールではないことだけ。

test(include_goal, include(missing), error(existence_error(procedure, include/1))).

% 7.4.2 ensure_loaded/1。指示だけ。同じパスは一度だけ読む。
% ファイルが要る確認は tests/cpp/ensure_test.cpp。ここはゴールではないことだけ。

test(ensure_goal, ensure_loaded(missing), error(existence_error(procedure, ensure_loaded/1))).

% 7.4.2 [File]。リストの各アトムを consult と同じ手続きで読む。
% ファイルが要る確認は tests/cpp/files_test.cpp。空リストは何も読まない。

:- [].

test(empty_files, true, true).

% 旗 argv は読み取り専用（initialization(_, main) と対。値は mosa の -- より後ろの引数）。

test(argv_readonly, set_prolog_flag(argv, []), error(permission_error(modify, flag, argv))).

% レビュー M10（iso-notes 7.4 の決定 A、SPEC 第5節の system モジュール）。組込述語と制御構造の節、本体が呼べない節は
% assertz/1 と同じ誤りで読み込みを失敗させ、その読み込みの節を戻す。誤りの Formal は tests/cpp/machine_test.cpp で見る。
% 前の consult で読んだファイルを同じパスで読み直さないよう、試験ごとにファイルを分ける。
m10_path(Name, Path) :- atom_concat('tests/tmp/m10_', Name, Stem), atom_concat(Stem, '.pl', Path).
m10_write(Path, Text) :- open(Path, write, S), write(S, Text), close(S).
m10_refused(Name, Text) :- m10_path(Name, Path), m10_write(Path, Text), \+ consult(Path), \+ current_predicate(m10_keep/1).
m10_loads(Name, Text) :- m10_path(Name, Path), m10_write(Path, Text), consult(Path).

test(m10_builtin_fact, (m10_refused(builtin_fact, 'm10_keep(1).\natom_length(_, 99).\n'), \+ current_predicate(atom_length/2), atom_length(abc, L)), solutions([L = 3])).
test(m10_builtin_rule, (m10_refused(builtin_rule, 'm10_keep(1).\nlength(_, 7) :- true.\n'), length([a], N)), solutions([N = 1])).
test(m10_call_rule, (m10_refused(call_rule, 'm10_keep(1).\ncall(_) :- write(mine).\n'), \+ current_predicate(call/1)), true).
test(m10_conj_head, m10_refused(conj_head, 'm10_keep(1).\n(m10_a, m10_b) :- true.\n'), true).
test(m10_disj_head, m10_refused(disj_head, 'm10_keep(1).\n(m10_a ; m10_b).\n'), true).
test(m10_cut_head, m10_refused(cut_head, 'm10_keep(1).\n! :- true.\n'), true).
test(m10_true_fact, m10_refused(true_fact, 'm10_keep(1).\ntrue.\n'), true).
test(m10_catch_fact, m10_refused(catch_fact, 'm10_keep(1).\ncatch(_, _, _).\n'), true).
test(m10_neck_head, m10_refused(neck_head, 'm10_keep(1).\n(m10_a :- m10_b) :- true.\n'), true).
test(m10_directive_head, m10_refused(directive_head, 'm10_keep(1).\n(:- m10_a) :- true.\n'), true).
test(m10_module_builtin, m10_refused(module_builtin, ':- module(m10_mod, []).\nm10_keep(1).\natom_length(_, 99).\n'), true).
test(m10_number_body, (m10_refused(number_body, 'm10_keep(1).\nm10_foo :- 1.\n'), \+ current_predicate(m10_foo/0)), true).
test(m10_conj_number_body, m10_refused(conj_number_body, 'm10_keep(1).\nm10_foo :- (true, 1).\n'), true).
test(m10_disj_number_body, m10_refused(disj_number_body, 'm10_keep(1).\nm10_foo :- (m10_q ; 2).\n'), true).
test(m10_float_body, m10_refused(float_body, 'm10_keep(1).\nm10_foo :- (m10_q -> 1.5 ; true).\n'), true).
% 変数の本体は call/1 になり（7.6.2）、call(1) と \+ 1 は呼んだときの誤り。読み込みでは断らない（assertz/1 と同じ）。
test(m10_var_body, (m10_loads(var_body, 'm10_vb(X) :- X.\n'), catch(m10_vb(1), error(E, _), true)), solutions([E = type_error(callable, 1)])).
test(m10_call_number, (m10_loads(call_number, 'm10_cn :- call(1).\n'), catch(m10_cn, error(E, _), true)), solutions([E = type_error(callable, 1)])).
test(m10_not_number, (m10_loads(not_number, 'm10_nn :- \\+ 1.\n'), catch(m10_nn, error(E, _), true)), solutions([E = type_error(callable, 1)])).
