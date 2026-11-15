% 条件付きコンパイル :- if/1、elif/1、else/0、endif/0（13211-1 に無い。SPEC 第3節、ADR 0027、iso-notes
% "Conditional compilation (not in 13211-1)"）。読み込み先は tests/pl/fixtures/cond_*.pl で、ゴールの相対パスは
% リポジトリの根から開く。対の誤りの字面、条件の誤りの Formal、halt の終了コードは tests/cpp/cond_compile_test.cpp。
:- dynamic(cond_seen/1).
:- dynamic(cond_called/1).
:- dynamic(cond_switch/0).
% 分岐の組み合わせ。取った分岐の節だけが残る。
:- if(true).
cond_pick(if).
:- else.
cond_pick(else).
:- endif.
:- if(fail).
cond_pick2(if).
:- elif(true).
cond_pick2(elif).
:- else.
cond_pick2(else).
:- endif.
:- if(fail).
cond_pick3(if).
:- elif(fail).
cond_pick3(elif).
:- else.
cond_pick3(else).
:- endif.
:- if(fail).
cond_pick4(if).
:- endif.
:- if(true).
:- if(fail).
cond_nest(inner_if).
:- else.
cond_nest(inner_else).
:- endif.
:- else.
:- if(true).
cond_nest(outer_else).
:- endif.
:- endif.
test(cond_if_then, findall(X, cond_pick(X), L), solutions([L = [if]])).
test(cond_elif_taken, findall(X, cond_pick2(X), L), solutions([L = [elif]])).
test(cond_else_taken, findall(X, cond_pick3(X), L), solutions([L = [else]])).
test(cond_none_taken, current_predicate(cond_pick4/1), fail).
test(cond_nested, findall(X, cond_nest(X), L), solutions([L = [inner_else]])).
% 前の分岐を取った後の elif と、読み飛ばし中に開いた if の条件は呼ばない。
:- if(true).
:- elif(assertz(cond_called(elif))).
:- endif.
:- if(fail).
:- if(assertz(cond_called(inner))).
:- endif.
:- endif.
test(cond_not_called, findall(X, cond_called(X), L), solutions([L = []])).
% 条件は 1 回だけ解き、残った選択点を切る（Review Focus 1）。member/2 は組み込みに無いので ; で書く。
:- if(((X = 1 ; X = 2), assertz(cond_seen(X)))).
:- endif.
test(cond_once, findall(X, cond_seen(X), L), solutions([L = [1]])).
% 読み飛ばす部分の指令は動かさず、節も足さない。横取りされる test/3 も渡さない（件数で見る）。
:- if(fail).
:- module(cond_never, []).
:- op(700, xfx, cond_op).
:- dynamic(cond_skip_dyn/1).
:- set_prolog_flag(double_quotes, atom).
:- include('no_such_cond_file.pl').
:- use_module(no_such_cond_module).
cond_skip_clause.
cond_skip_rule --> [a].
cond_bad_rule --> 1.
test(cond_skipped_test, fail, true).
:- endif.
test(cond_skip_op, current_op(_, _, cond_op), fail).
test(cond_skip_dynamic, catch(cond_skip_dyn(_), error(existence_error(procedure, cond_skip_dyn/1), _), true), true).
test(cond_skip_flag, (X = "ab", atom(X)), fail).
test(cond_skip_clause, current_predicate(cond_skip_clause/0), fail).
test(cond_skip_rule, current_predicate(cond_skip_rule/2), fail).
% 読み飛ばす部分の構文の誤りは、診断を出さずに次の終止符から読み続ける（問い 3 の A）。
:- if(fail).
cond_third(1r3).
:- endif.
cond_after_syntax.
test(cond_skip_syntax, cond_after_syntax, true).
% 方言の分岐（Review Focus 5）。dialect は mosa なので else の分岐を取り、無い library(clpfd) は読まない。
:- if(current_prolog_flag(dialect, swi)).
:- use_module(library(clpfd)).
cond_dialect(swi).
:- else.
cond_dialect(other).
:- endif.
test(cond_dialect, findall(X, cond_dialect(X), L), solutions([L = [other]])).
% 失敗する読み込み。対の誤りと、閉じない if と、条件の例外は読み込みを失敗させる。構造の誤りは読み飛ばし中の
% 入れ子の節でも失敗させる（計画の「設計との違い」1）。取った分岐の構文の誤りは今と同じく失敗する。
test(cond_stray_endif, consult('tests/pl/fixtures/cond_stray_endif.pl'), fail).
test(cond_stray_else, consult('tests/pl/fixtures/cond_stray_else.pl'), fail).
test(cond_stray_elif, consult('tests/pl/fixtures/cond_stray_elif.pl'), fail).
test(cond_elif_after_else, consult('tests/pl/fixtures/cond_elif_after_else.pl'), fail).
test(cond_else_twice, consult('tests/pl/fixtures/cond_else_twice.pl'), fail).
test(cond_unclosed, consult('tests/pl/fixtures/cond_unclosed.pl'), fail).
test(cond_unclosed_include, consult('tests/pl/fixtures/cond_unclosed_include.pl'), fail).
test(cond_endif_in_include, consult('tests/pl/fixtures/cond_endif_in_include.pl'), fail).
test(cond_throw, consult('tests/pl/fixtures/cond_throw.pl'), fail).
test(cond_var, consult('tests/pl/fixtures/cond_var.pl'), fail).
test(cond_number, consult('tests/pl/fixtures/cond_number.pl'), fail).
test(cond_taken_syntax, consult('tests/pl/fixtures/cond_taken_syntax.pl'), fail).
test(cond_swallowed_endif, consult('tests/pl/fixtures/cond_swallowed_endif.pl'), fail).
test(cond_skipped_nested_bad, consult('tests/pl/fixtures/cond_skipped_nested_bad.pl'), fail).
% 条件は読み込み中のモジュールで解く。
test(cond_module, (use_module('tests/pl/fixtures/cond_module.pl'), cond_where(X)), solutions([X = module])).
% 分岐が変わった読み直しは、前に取った分岐だけが定義した述語を消す（Review Focus 3、ADR 0020）。
test(cond_reload_on, (assertz(cond_switch), consult('tests/pl/fixtures/cond_reload.pl'), cond_only_on), true).
test(cond_reload_off, (retractall(cond_switch), consult('tests/pl/fixtures/cond_reload.pl'), \+ current_predicate(cond_only_on/0), cond_always), true).
