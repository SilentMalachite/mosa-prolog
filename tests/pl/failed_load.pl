% 失敗した consult の巻き戻し（consult/1 は ISO に無い。テキストは 7.4）。レビュー M24、ADR 0017。
% 読み込みの中身は tests/tmp/ に書く。
m24_write(Path, Text) :- open(Path, write, S), write(S, Text), close(S).
% 失敗した読み込みは、テキスト（節と宣言）がしたことを取り消し、指令のゴールがしたことは残す（レビュー M24、
% ADR 0017）。テキストが触れた定義は触れる前へ戻し、テキストが始めた定義は消す。ゴールが assert で作った述語と、
% ゴールが既にある述語にした変更は残る。読み込みは選択点の無いゴールで動かす。\+ や findall の下では、既にある
% 述語の節を足す所で読み込みが断られ、:- fail. まで届かない（M24 の敵対的レビュー A4）。そこで :- fail. の直前で
% m24_reached に印を足し、次のテストで印と結果を確かめる。
:- dynamic(m24_reached/1).
m24_load(Name, Body) :-
    atom_concat('tests/tmp/m24_', Name, Base), atom_concat(Base, '.pl', Path),
    open(Path, write, S), write(S, Body), writeq(S, (:- assertz(user:m24_reached(Name)))), write(S, '.\n:- fail.\n'),
    close(S), consult(Path).
m24_left(Name, Goal, Left) :- m24_reached(Name), findall(Goal, Goal, Left).
:- dynamic(m24_q/1).
m24_q(1).
test(m24_reassert_load, m24_load(reassert, ':- abolish(m24_q/1).\n:- assertz(m24_q(7)).\n'), fail).
test(m24_reassert, m24_left(reassert, m24_q(_), L), solutions([L = [m24_q(7)]])).
:- dynamic(m24_s/1).
m24_s(1).
test(m24_reassert_clause_load, m24_load(reassert_clause, ':- abolish(m24_s/1).\n:- assertz(m24_s(7)).\nm24_s(8).\n'), fail).
test(m24_reassert_clause, (m24_left(reassert_clause, m24_s(_), L), assertz(m24_s(9))), solutions([L = [m24_s(7)]])).
test(m24_new_by_assert_load, m24_load(new_by_assert, ':- assertz(m24_n(1)).\n'), fail).
test(m24_new_by_assert, m24_left(new_by_assert, m24_n(_), L), solutions([L = [m24_n(1)]])).
test(m24_assert_then_text_load, m24_load(assert_then_text, ':- assertz(m24_u(1)).\nm24_u(2).\n'), fail).
test(m24_assert_then_text, (m24_left(assert_then_text, m24_u(_), L), assertz(m24_u(3))), solutions([L = [m24_u(1)]])).
test(m24_text_made_load, m24_load(text_made, ':- dynamic(m24_d/1).\n:- assertz(m24_d(1)).\nm24_t(1).\n'), fail).
test(m24_text_made, (m24_reached(text_made), \+ current_predicate(m24_d/1), \+ current_predicate(m24_t/1)), true).
:- dynamic(m24_e/1).
m24_e(1).
test(m24_goal_changes_load, m24_load(goal_changes, ':- assertz(m24_e(2)).\n:- retract(m24_e(1)).\n'), fail).
test(m24_goal_changes, m24_left(goal_changes, m24_e(_), L), solutions([L = [m24_e(2)]])).
test(m24_text_after_abolish_load, (assertz(m24_a(1)), m24_load(text_after_abolish, ':- abolish(m24_a/1).\nm24_a(8).\n')), fail).
test(m24_text_after_abolish, (m24_reached(text_after_abolish), \+ current_predicate(m24_a/1)), true).
% 失敗した読み込みが宣言したモジュールは戻るので、そのモジュールへゴールが assert した述語も残さない。
test(m24_module_load, m24_load(module, ':- module(m24_mod, []).\n:- assertz(m24_mf(1)).\n'), fail).
test(m24_module, (m24_reached(module), \+ current_predicate(m24_mod:m24_mf/1)), true).
% 入れ子の読み込みのテキストも、親が失敗すれば取り消す。指令の [File] と consult(File) は、読んでいるファイルの
% ディレクトリから探す。子は成功したら写しを親へ渡す（M24 の敵対的レビュー A1）。
% 子のゴールが作った述語は残り、子のテキストが作った述語は消える。
test(m24_nested_goal_load, (m24_write('tests/tmp/m24_child1.pl', 'm24_n1(2).\n'), m24_load(nested_goal, ':- assertz(m24_n1(1)).\n:- consult(\'tests/tmp/m24_child1.pl\'), true.\n')), fail).
test(m24_nested_goal, (m24_left(nested_goal, m24_n1(_), L), assertz(m24_n1(3))), solutions([L = [m24_n1(1)]])).
test(m24_nested_load, (m24_write('tests/tmp/m24_child2.pl', ':- assertz(m24_n2(1)).\nm24_n2(2).\nm24_nx(1).\n'), m24_load(nested, ':- consult(\'m24_child2.pl\').\n')), fail).
test(m24_nested, (m24_left(nested, m24_n2(_), L), \+ current_predicate(m24_nx/1), assertz(m24_n2(3))), solutions([L = [m24_n2(1)]])).
% 入れ子の consult の指令は、もう全部の述語を写さない。その後のゴールが既にある述語にした変更も残る（同 A5）。
:- dynamic(m24_e5/1).
m24_e5(1).
test(m24_after_nested_load, (m24_write('tests/tmp/m24_empty.pl', ''), m24_load(after_nested, ':- [\'m24_empty.pl\'].\n:- assertz(m24_e5(3)).\n')), fail).
test(m24_after_nested, m24_left(after_nested, m24_e5(_), L), solutions([L = [m24_e5(1), m24_e5(3)]])).
% テキストが先に触れた定義は、テキストが触れる前へ戻すので、その後のゴールの変更も一緒に戻る（ADR 0017 の影響）。
:- dynamic(m24_t2/1).
m24_t2(1).
test(m24_text_first_load, m24_load(text_first, ':- dynamic(m24_t2/1).\nm24_t2(8).\n:- assertz(m24_t2(7)).\n'), fail).
test(m24_text_first, m24_left(text_first, m24_t2(_), L), solutions([L = [m24_t2(1)]])).
% 写しは、写した時と同じ定義が残る述語だけへ戻す。ゴールが abolish して作り直した述語へ前の写しを戻さず、テキストが
% abolish 済みの枠を使い直した定義は、その前の写し（ゴールの abolish の後の枠）へ戻す（M24 の Codex レビュー P2）。
% 開いた走査の下の abolish（枠が残る）でも、走査の無い abolish と同じ答えになる（同 A3）。
test(m24_abolished_shot_load, m24_load(abolished_shot, ':- assertz(m24_p1(1)).\n:- dynamic(m24_p1/1).\n:- abolish(m24_p1/1).\n:- assertz(m24_p1(7)).\n'), fail).
test(m24_abolished_shot, m24_left(abolished_shot, m24_p1(_), L), solutions([L = [m24_p1(7)]])).
test(m24_reuse_frame_load, m24_load(reuse_frame, ':- dynamic(m24_p2/1).\n:- abolish(m24_p2/1).\n:- assertz(m24_p2(1)).\n:- assertz(m24_p2(2)).\n:- (m24_p2(_), abolish(m24_p2/1)).\nm24_p2(8).\n'), fail).
test(m24_reuse_frame, (m24_reached(reuse_frame), \+ current_predicate(m24_p2/1)), true).
:- dynamic(m24_p3/1).
m24_p3(1). m24_p3(2).
test(m24_reuse_old_frame_load, m24_load(reuse_old_frame, ':- (m24_p3(_), abolish(m24_p3/1)).\nm24_p3(8).\n'), fail).
test(m24_reuse_old_frame, (m24_reached(reuse_old_frame), \+ current_predicate(m24_p3/1)), true).
:- dynamic(m24_p4/1).
m24_p4(1).
test(m24_revive_load, m24_load(revive, ':- dynamic(m24_p4/1).\n:- (m24_p4(_), abolish(m24_p4/1)).\n:- assertz(m24_p4(7)).\n'), fail).
test(m24_revive, m24_left(revive, m24_p4(_), L), solutions([L = [m24_p4(7)]])).
:- dynamic(m24_p5/1).
m24_p5(1).
test(m24_abolish_open_load, m24_load(abolish_open, ':- dynamic(m24_p5/1).\n:- (m24_p5(_), abolish(m24_p5/1)).\n'), fail).
test(m24_abolish_open, (m24_reached(abolish_open), \+ current_predicate(m24_p5/1)), true).
:- dynamic(m24_p6/1).
m24_p6(1).
test(m24_goal_remake_load, m24_load(goal_remake, ':- dynamic(m24_p6/1).\nm24_p6(8).\n:- abolish(m24_p6/1).\n:- assertz(m24_p6(9)).\nm24_p6(10).\n'), fail).
test(m24_goal_remake, (m24_left(goal_remake, m24_p6(_), L), assertz(m24_p6(11))), solutions([L = [m24_p6(9)]])).
test(m24_text_frame_remake_load, m24_load(text_frame_remake, ':- dynamic(m24_r3/1).\nm24_r3(1).\n:- abolish(m24_r3/1), assertz(m24_r3(3)).\n'), fail).
test(m24_text_frame_remake, m24_left(text_frame_remake, m24_r3(_), L), solutions([L = [m24_r3(3)]])).
test(m24_text_frame_revive_load, m24_load(text_frame_revive, ':- dynamic(m24_r4/1).\nm24_r4(1).\n:- m24_r4(_), abolish(m24_r4/1), assertz(m24_r4(3)).\n'), fail).
test(m24_text_frame_revive, m24_left(text_frame_revive, m24_r4(_), L), solutions([L = [m24_r4(3)]])).
% テキストが始めた定義の写し（子が親のテキストの定義を写したものなど）は戻す先にしない。その定義も取り消すので、
% 戻すと枠ごと消え、ゴールの abolish の下でまだ開いている走査が残りの節を失う（M24 の 2 度目の Codex レビュー P2）。
:- dynamic(m24_o1/1).
m24_o1(1). m24_o1(2).
test(m24_scan_child, (m24_write('tests/tmp/m24_child3.pl', ':- dynamic(m24_o1/1).\n'), findall(X, (m24_o1(X), (X =:= 1 -> \+ m24_load(scan_child, ':- abolish(m24_o1/1).\n:- dynamic(m24_o1/1).\n:- consult(\'m24_child3.pl\').\n') ; true)), L), m24_reached(scan_child)), solutions([L = [1, 2]])).
% 子の initialization のゴールが読んだ孫のテキストも、親が失敗すれば取り消す（同 P2）。
m24_g(1).
test(m24_init_grandchild_load, (m24_write('tests/tmp/m24_gc4.pl', 'm24_g(2).\n'), m24_write('tests/tmp/m24_child4.pl', ':- initialization(consult(\'tests/tmp/m24_gc4.pl\')).\n'), m24_load(init_grandchild, ':- consult(\'m24_child4.pl\').\n')), fail).
test(m24_init_grandchild, m24_left(init_grandchild, m24_g(_), L), solutions([L = [m24_g(1)]])).
% 失敗したテキストの meta_predicate の保留は、ゴールが assert で作った述語にも残さない（同 P2）。
test(m24_goal_meta_load, m24_load(goal_meta, ':- meta_predicate(m24_mp(0)).\n:- assertz(m24_mp(a)).\n'), fail).
test(m24_goal_meta, (m24_reached(goal_meta), \+ predicate_property(m24_mp(_), meta_predicate(_)), m24_mp(a)), true).
% [File] と consult(File) の指令の子が失敗したら、子のゴールの abolish も残る。前はリストの前の節を名前で戻し、
% abolish 済みの印を残したまま生きた節を戻した。呼べないのに次の assertz が古い節を連れて戻った
% （M24 の敵対的レビュー A1）。子のテキストは子が、成功した前のファイルのテキストは親の巻き戻しが取り消す。
:- dynamic(m24_z/1).
m24_z(1). m24_z(2).
test(m24_child_scan_abolish_load, (m24_write('tests/tmp/m24_za.pl', ':- m24_z(_), abolish(m24_z/1).\nx(.\n'), m24_write('tests/tmp/m24_zp.pl', ':- consult(\'m24_za.pl\').\n'), consult('tests/tmp/m24_zp.pl')), fail).
test(m24_child_scan_abolish, (\+ current_predicate(m24_z/1), catch(m24_z(_), error(existence_error(procedure, _), _), true), assertz(m24_z(3)), findall(X, m24_z(X), L)), solutions([L = [3]])).
:- dynamic(m24_y/1).
m24_y(1). m24_y(2).
test(m24_list_outer_scan, (m24_write('tests/tmp/m24_yb.pl', ':- abolish(m24_y/1).\nx(.\n'), m24_write('tests/tmp/m24_yp.pl', ':- [\'m24_yb.pl\'].\n'), findall(X, (m24_y(X), (X =:= 1 -> \+ consult('tests/tmp/m24_yp.pl') ; true)), L), \+ current_predicate(m24_y/1), assertz(m24_y(3)), findall(Y, m24_y(Y), L2)), solutions([(L = [1, 2], L2 = [3])])).
% リストの前のファイルが成功し、後のファイルが失敗したら、前のファイルのテキストも取り消す。
:- dynamic(m24_w/1).
m24_w(1).
test(m24_list_earlier_load, (m24_write('tests/tmp/m24_w1.pl', 'm24_w(2).\nm24_wn(1).\n'), m24_write('tests/tmp/m24_w2.pl', 'x(.\n'), m24_write('tests/tmp/m24_wp.pl', ':- [\'m24_w1.pl\', \'m24_w2.pl\'].\n'), consult('tests/tmp/m24_wp.pl')), fail).
test(m24_list_earlier, (findall(X, m24_w(X), L), \+ current_predicate(m24_wn/1)), solutions([L = [1]])).
% 親が保留した meta_predicate を子のゴールが作った述語が引き取っても、親の宣言として残る。子が失敗して述語が
% 残ると、親の次の節が子の述語を置き換えるときに宣言を消していた（同 A2）。子が成功したときも同じ。
test(m24_parent_meta_failed_child, (m24_write('tests/tmp/m24_mqc.pl', ':- assertz(m24_mq(true)).\n:- fail.\n'), m24_write('tests/tmp/m24_mqp.pl', ':- meta_predicate(m24_mq(0)).\n:- (consult(\'tests/tmp/m24_mqc.pl\') ; true).\nm24_mq(x).\n'), consult('tests/tmp/m24_mqp.pl'), predicate_property(m24_mq(_), meta_predicate(M)), findall(X, clause(m24_mq(X), true), L)), solutions([(M = m24_mq(0), L = [x])])).
test(m24_parent_meta_child, (m24_write('tests/tmp/m24_mrc.pl', ':- assertz(m24_mr(true)).\n'), m24_write('tests/tmp/m24_mrp.pl', ':- meta_predicate(m24_mr(0)).\n:- consult(\'tests/tmp/m24_mrc.pl\'), true.\nm24_mr(x).\n'), consult('tests/tmp/m24_mrp.pl'), predicate_property(m24_mr(_), meta_predicate(M))), solutions([M = m24_mr(0)])).
