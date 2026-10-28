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
test(dc_only_call, dc_only(_), error(existence_error(procedure, dc_only/1))).
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

test(mf_same, (findall(X, mf_p(X), L), L == [1, 2]), true).
test(mf_list_a, (findall(X, mf_a(X), L), L == [1, 2]), true).
test(mf_list_b, (findall(t, mf_b, L), L == [t]), true).
test(mf_rule, (findall(X, mf_rule(X), L), L == [1, 9]), true).
test(mf_only_absent, current_predicate(mf_only/1), fail).
test(mf_only_call, mf_only(_), error(existence_error(procedure, mf_only/1))).
test(mf_static, assertz(mf_p(3)), error(permission_error(modify, static_procedure, mf_p/1))).

% 7.4.2 initialization/1。指示だけ。ゴールはテキストを準備したあとに、ソースの順で一度実行する。
% 失敗はエラーにしない。捕捉されない例外は読み込みを失敗させ、節は残す。

:- dynamic(init_s/1).
:- initialization((retractall(init_s(_)), assertz(init_s(1)))).

test(init_ran, (findall(X, init_s(X), L), L == [1]), true).
test(init_goal, initialization(true), error(existence_error(procedure, initialization/1))).

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
