// レビュー L11（ADR 0020、iso-notes 7.4.2 の決定 A）。同じパスを読み直すと、前にそのパスが定義し新しい中身に無い
// 述語は消える。失敗した読み直し、ゴールが作った述語、multifile の述語、後で別のファイルが定義した述語は残る。
// 入れ子の consult の古い述語は、一番外の読み込みが成功したときにだけ消える。
#include "harness.hpp"

#include "mosa/machine.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <string_view>

namespace {

std::string tmp_path(std::string_view name) { return std::string(MOSA_SOURCE_DIR) + "/tests/tmp/" + std::string{name}; }

void write_file(const std::string& path, std::string_view text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
}

bool holds(mosa::Machine& machine, const std::string& goal) {
  return machine.query(goal).kind == mosa::StopKind::Success;
}

// 述語が無ければ existence_error になる。節の無い dynamic の述語は失敗するだけなので、ここで区別できる。
bool gone(mosa::Machine& machine, const std::string& goal, const std::string& indicator) {
  return holds(machine, "catch((" + goal + ", R = there), error(existence_error(procedure, " + indicator +
                            "), _), R = gone), R == gone.");
}

void check_reconsult_removes() {
  const std::string path = tmp_path("reconsult_removes.pl");
  write_file(path, ":- dynamic(d/1).\nd(1).\np(1).\nq(1).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(gone(*machine, "never_defined", "never_defined/0"), "gone sees an unknown procedure");
  mosa_test::check(machine->consult(path), "reconsult first load");
  mosa_test::check(!gone(*machine, "p(_)", "p/1"), "gone sees a defined procedure");
  mosa_test::check(holds(*machine, "assertz(d(5))."), "reconsult assert into the file's dynamic");
  write_file(path, "q(2).\n");
  mosa_test::check(machine->consult(path), "reconsult second load");
  mosa_test::check(holds(*machine, "q(2).") && !holds(*machine, "q(1)."), "reconsult replaces q/1");
  mosa_test::check(gone(*machine, "p(_)", "p/1"), "reconsult removes static p/1");
  mosa_test::check(gone(*machine, "d(_)", "d/1"), "reconsult removes dynamic d/1 with its asserted clause");
  // consult/1 のゴールからの読み直しも同じ（run_consult）。
  write_file(path, "r.\n");
  mosa_test::check(holds(*machine, "consult('" + path + "')."), "reconsult by consult/1");
  mosa_test::check(gone(*machine, "q(_)", "q/1") && holds(*machine, "r."), "consult/1 removes q/1");
}

void check_reconsult_failure_keeps() {
  const std::string path = tmp_path("reconsult_failure.pl");
  write_file(path, "p(1).\nq(1).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(path), "failed reconsult first load");
  write_file(path, "q(2).\nbroken(\n");
  mosa_test::check(!machine->consult(path), "failed reconsult fails");
  mosa_test::check(holds(*machine, "p(1).") && holds(*machine, "q(1)."), "failed reconsult keeps the old definitions");
}

void check_reconsult_keeps_others() {
  const std::string a = tmp_path("reconsult_a.pl");
  const std::string b = tmp_path("reconsult_b.pl");
  write_file(a, ":- assertz(g(1)).\np(a).\n:- multifile(m/1).\nm(a).\n");
  write_file(b, "p(b).\n:- multifile(m/1).\nm(b).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(a) && machine->consult(b), "keeps others load");
  write_file(a, "x.\n");
  mosa_test::check(machine->consult(a), "keeps others reconsult");
  mosa_test::check(holds(*machine, "g(1)."), "reconsult keeps a predicate made by a goal");
  mosa_test::check(holds(*machine, "p(b).") && !holds(*machine, "p(a)."), "reconsult keeps what another file defined");
  mosa_test::check(holds(*machine, "m(b)."), "reconsult keeps a multifile predicate");
}

// 入れ子の consult の古い述語は、外の読み込みが成功すれば消え、失敗すれば外の読み込みと一緒に戻る。
void check_reconsult_nested() {
  const std::string parent = tmp_path("reconsult_parent.pl");
  const std::string child = tmp_path("reconsult_child.pl");
  write_file(parent, ":- consult('reconsult_child.pl').\n");
  write_file(child, "c1.\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(parent), "nested first load");
  write_file(child, "c2.\n");
  write_file(parent, ":- consult('reconsult_child.pl').\nbroken(\n");
  mosa_test::check(!machine->consult(parent), "nested reconsult with a broken parent fails");
  mosa_test::check(holds(*machine, "c1.") && gone(*machine, "c2", "c2/0"), "failed parent keeps the child's old text");
  write_file(parent, ":- consult('reconsult_child.pl').\n");
  mosa_test::check(machine->consult(parent), "nested reconsult");
  mosa_test::check(gone(*machine, "c1", "c1/0") && holds(*machine, "c2."), "nested reconsult removes c1/0");
}

// 消した述語の discontiguous の宣言も消え、呼べば existence_error になる。宣言し直していれば、初めて読んだときと
// 同じく宣言だけの手続きとして失敗する（6 度目の Codex レビュー P2）。
void check_reconsult_discontiguous() {
  const std::string f = tmp_path("reconsult_discontiguous.pl");
  const std::string g = tmp_path("reconsult_discontiguous_again.pl");
  write_file(f, ":- discontiguous(p/0).\np.\n");
  write_file(g, ":- discontiguous(r/0).\nr.\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(f) && machine->consult(g), "discontiguous load");
  write_file(f, "q.\n");
  write_file(g, ":- discontiguous(r/0).\nq2.\n");
  mosa_test::check(machine->consult(f) && machine->consult(g), "discontiguous reconsult");
  mosa_test::check(gone(*machine, "p", "p/0"), "reconsult drops the old discontiguous declaration");
  mosa_test::check(holds(*machine, "catch((r -> R = yes ; R = no), error(existence_error(_, _), _), R = err), R == no."),
                   "reconsult keeps a discontiguous declaration made again");
}

// include したテキストの述語は読んだファイルのもので、include をやめれば消える。ensure_loaded したファイルの述語は
// そのファイルのもので、読み直しが ensure_loaded を飛ばしても残る。
void check_reconsult_include_ensure() {
  const std::string top = tmp_path("reconsult_top.pl");
  write_file(tmp_path("reconsult_inc.pl"), "i1.\n");
  write_file(tmp_path("reconsult_once.pl"), "e1.\n");
  write_file(top, ":- include('reconsult_inc.pl').\n:- ensure_loaded('reconsult_once.pl').\nt.\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(top), "include first load");
  write_file(top, ":- ensure_loaded('reconsult_once.pl').\nt.\n");
  mosa_test::check(machine->consult(top), "include reconsult");
  mosa_test::check(gone(*machine, "i1", "i1/0"), "reconsult removes what the dropped include defined");
  mosa_test::check(holds(*machine, "e1.") && holds(*machine, "t."), "reconsult keeps the ensure_loaded file's predicate");
}

// 読み直しのゴールが節を足した述語は残る（ゴールがしたことは残す）。初期化のゴールが consult したファイルも、
// 一番外の読み込みが古い述語を消す。
void check_reconsult_goals() {
  const std::string path = tmp_path("reconsult_goal.pl");
  const std::string late = tmp_path("reconsult_late.pl");
  write_file(path, ":- dynamic(p/1).\np(1).\n:- initialization(consult('" + late + "')).\n");
  write_file(late, "l1.\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(path), "goals first load");
  write_file(path, ":- assertz(p(2)).\n:- initialization(consult('" + late + "')).\n");
  write_file(late, "l2.\n");
  mosa_test::check(machine->consult(path), "goals reconsult");
  // 前の中身の p(1) は消え、新しい中身を初めて読んだのと同じになる（敵対的レビュー A2）。
  mosa_test::check(holds(*machine, "findall(X, p(X), L), L == [2]."), "reconsult keeps only what a directive asserted");
  mosa_test::check(gone(*machine, "l1", "l1/0") && holds(*machine, "l2."), "consult from an initialization goal");
}

// 持ち主は最後に定義したテキスト。同じ読み込みで別のファイルが dynamic と宣言し直した述語と、文字だけの読み込みが
// 置き換えた述語は、前のファイルを読み直しても消えない（L11 の Codex レビュー P2）。
void check_reconsult_later_owner() {
  const std::string a = tmp_path("reconsult_owner_a.pl");
  write_file(tmp_path("reconsult_owner_b.pl"), ":- dynamic(p/1).\n");
  write_file(a, ":- dynamic(p/1).\n:- ensure_loaded('reconsult_owner_b.pl').\nq(1).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(a), "later owner load");
  mosa_test::check(machine->load_program("q(2).\n").ok, "later owner text load");
  write_file(a, "x.\n");
  mosa_test::check(machine->consult(a), "later owner reconsult");
  mosa_test::check(!gone(*machine, "p(_)", "p/1"), "reconsult keeps what another file declared again");
  mosa_test::check(holds(*machine, "q(2)."), "reconsult keeps what a text load replaced");
}

// 宣言だけを残した中身は、前の節を消して述語を残す（敵対的レビュー A1）。
void check_reconsult_declaration_only() {
  const std::string path = tmp_path("reconsult_declared.pl");
  write_file(path, ":- dynamic(d/1).\nd(1).\nd(2).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(path), "declaration only first load");
  write_file(path, ":- dynamic(d/1).\n");
  mosa_test::check(machine->consult(path), "declaration only reconsult");
  mosa_test::check(holds(*machine, "findall(X, d(X), L), L == []."), "declaration only reconsult drops old clauses");
  mosa_test::check(!gone(*machine, "d(_)", "d/1"), "declaration only reconsult keeps d/1");
  // 読み直しのゴールが足して消した述語も、空の動的な述語として残る（3 度目の Codex レビュー P2）。
  const std::string other = tmp_path("reconsult_retracted.pl");
  write_file(other, ":- dynamic(p/1).\np(1).\n");
  mosa_test::check(machine->consult(other), "retracted first load");
  write_file(other, ":- assertz(p(2)).\n:- retract(p(2)).\n");
  mosa_test::check(machine->consult(other), "retracted reconsult");
  mosa_test::check(holds(*machine, "findall(X, p(X), L), L == [].") && !gone(*machine, "p(_)", "p/1"),
                   "reconsult keeps a predicate a directive asserted and retracted");
  // 失敗した入れ子の consult がその述語を巻き戻しても同じ（7 度目の Codex レビュー P2）。
  write_file(tmp_path("reconsult_bad.pl"), "p(3).\nbroken(\n");
  write_file(other, ":- dynamic(p/1).\np(1).\n");
  mosa_test::check(machine->consult(other), "rolled back first load");
  write_file(other, ":- assertz(p(2)), retractall(p(_)).\n:- findall(_, consult('" + tmp_path("reconsult_bad.pl") +
                        "'), _).\n");
  mosa_test::check(machine->consult(other), "rolled back reconsult");
  mosa_test::check(holds(*machine, "findall(X, p(X), L), L == [].") && !gone(*machine, "p(_)", "p/1"),
                   "reconsult keeps a changed predicate a failed nested load rolled back");
}

// 初期化のゴールの例外は節を残す。直に読んでも入れ子で読んでも、残した節の持ち主を付け、古い述語を消す（敵対的
// レビュー A3）。
void check_reconsult_init_raises() {
  const std::string a = tmp_path("reconsult_raise_a.pl");
  const std::string c = tmp_path("reconsult_raise_c.pl");
  write_file(c, ":- initialization(throw(oops)).\n");
  write_file(a, "newp.\n:- consult('reconsult_raise_c.pl').\nafter.\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->consult(a), "nested initialization raises");
  mosa_test::check(holds(*machine, "newp."), "a raising initialization keeps the text");
  write_file(c, "cc.\n");
  write_file(a, ":- consult('reconsult_raise_c.pl').\nother.\n");
  mosa_test::check(machine->consult(a), "reconsult after a raising initialization");
  mosa_test::check(gone(*machine, "newp", "newp/0"), "reconsult removes what a kept load defined");
  const std::string f = tmp_path("reconsult_raise_f.pl");
  const std::string top = tmp_path("reconsult_raise_top.pl");
  write_file(f, "p.\nq.\n");
  write_file(top, ":- consult('reconsult_raise_f.pl').\n");
  mosa_test::check(machine->consult(f) && machine->consult(top), "raising reconsult first load");
  write_file(f, "r.\n:- initialization(throw(x)).\n");
  mosa_test::check(!machine->consult(top), "raising reconsult through a parent fails");
  mosa_test::check(gone(*machine, "q", "q/0") && holds(*machine, "r."), "raising reconsult through a parent");
}

// 入れ子の consult の後で親が同じ述語の節を続けると、その述語は親のものになる（敵対的レビュー A5）。
void check_reconsult_parent_continues() {
  const std::string a = tmp_path("reconsult_cont_a.pl");
  const std::string c = tmp_path("reconsult_cont_c.pl");
  write_file(a, "p(1).\n:- consult('reconsult_cont_c.pl').\np(3).\n");
  write_file(c, "p(2).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(a), "parent continues load");
  write_file(c, "other.\n");
  mosa_test::check(machine->consult(c), "parent continues reconsult child");
  mosa_test::check(holds(*machine, "p(3)."), "reconsulting the child keeps the parent's clauses");
}

// 走っている古い述語の中から読み直すと、開いた走査は論理的更新の見方で残りの節を試し、その後は述語が無い。
// モジュールのファイルの読み直しも、もう定義しない局所の述語を消す。
void check_reconsult_open_and_module() {
  const std::string f = tmp_path("reconsult_open.pl");
  write_file(f, "s(1).\ns(2).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(f), "open scan load");
  write_file(f, "x.\n");
  mosa_test::check(holds(*machine, "findall(X, (s(X), (X == 1 -> consult('" + f + "') ; true)), L), L == [1, 2]."),
                   "open scan keeps its clauses");
  mosa_test::check(gone(*machine, "s(_)", "s/1"), "open scan reconsult removes s/1");
  // 開いた走査のために残した静的な述語の枠へも、新しい動的な定義を始められる（2 度目の Codex レビュー P2）。
  const std::string g = tmp_path("reconsult_open_static.pl");
  write_file(g, "t(1).\nt(2).\n");
  mosa_test::check(machine->consult(g), "open static load");
  write_file(g, "y.\n");
  mosa_test::check(holds(*machine, "t(_), consult('" + g + "'), assertz(t(3)), !, findall(Y, t(Y), L), L == [3]."),
                   "assert into a removed static predicate with an open scan");
  const std::string m = tmp_path("reconsult_module.pl");
  write_file(m, ":- module(rm, [f/0]).\nf.\ng.\n");
  mosa_test::check(machine->consult(m), "module load");
  write_file(m, ":- module(rm, [f/0]).\nf.\n");
  mosa_test::check(machine->consult(m), "module reconsult");
  mosa_test::check(holds(*machine, "f.") && gone(*machine, "rm:g", "_"), "module reconsult removes rm:g/0");
}

// 4 度目の Codex レビューの P2。新しいテキストが古い述語へした meta_predicate の宣言は、述語を消しても初期化の
// ゴールが作り直した定義に残る。同じファイルを初期化のゴールの前と中で読んでも、1 回の読みと数える。
void check_reconsult_meta_and_phases() {
  const std::string f = tmp_path("reconsult_meta.pl");
  write_file(f, "p(_).\n");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(f), "meta first load");
  write_file(f, ":- meta_predicate(p(0)).\n:- initialization(assertz((p(G) :- call(G)))).\n");
  mosa_test::check(machine->consult(f), "meta reconsult");
  mosa_test::check(holds(*machine, "predicate_property(p(_), meta_predicate(p(0)))."), "reconsult keeps a new meta");
  // 入れ子の consult が宣言しても同じ（5 度目の Codex レビュー P2）。
  const std::string pa = tmp_path("reconsult_meta_a.pl");
  const std::string pb = tmp_path("reconsult_meta_b.pl");
  write_file(pb, "r(_).\n");
  write_file(pa, ":- consult('reconsult_meta_b.pl').\n");
  mosa_test::check(machine->consult(pa), "nested meta first load");
  write_file(pb, ":- meta_predicate(r(0)).\n");
  write_file(pa, ":- consult('reconsult_meta_b.pl').\n:- initialization(assertz((r(G) :- call(G)))).\n");
  mosa_test::check(machine->consult(pa), "nested meta reconsult");
  mosa_test::check(holds(*machine, "predicate_property(r(_), meta_predicate(r(0)))."), "nested reconsult keeps a meta");
  const std::string top = tmp_path("reconsult_phase_top.pl");
  const std::string b = tmp_path("reconsult_phase_b.pl");
  write_file(b, ":- dynamic(q/1).\n");
  write_file(top, ":- consult('reconsult_phase_b.pl').\n:- initialization(consult('" + b + "')).\n");
  mosa_test::check(machine->consult(top), "phases first load");
  write_file(b, ":- dynamic(q/1).\n");
  write_file(top, ":- consult('reconsult_phase_b.pl').\n"
                  ":- open('" + b + "', write, S), write(S, 'x.'), nl(S), close(S).\n"
                  ":- initialization(consult('" + b + "')).\n");
  mosa_test::check(machine->consult(top), "phases reconsult");
  mosa_test::check(!gone(*machine, "q(_)", "q/1"), "a file read before and during initialization counts once");
}

}  // namespace

int main() {
  check_reconsult_removes();
  check_reconsult_failure_keeps();
  check_reconsult_keeps_others();
  check_reconsult_nested();
  check_reconsult_discontiguous();
  check_reconsult_include_ensure();
  check_reconsult_goals();
  check_reconsult_later_owner();
  check_reconsult_declaration_only();
  check_reconsult_init_raises();
  check_reconsult_parent_continues();
  check_reconsult_open_and_module();
  check_reconsult_meta_and_phases();
  return mosa_test::failures();
}
