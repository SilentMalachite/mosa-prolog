// フェーズ12 の Muse レビュー（Major 1）。読み込みの途中で C++ の例外（ヒープの上限）が抜けても、読み込みの状態を
// 残さない（ADR 0017 の巻き戻しを、失敗と同じく例外でもかける）。前は 3 つが残った。ゴールの読み込みが替えた
// load_module が戻らず、後の節と取り込みが呼んだ文脈のモジュールへ入った。途中まで読んだモジュールと準備済みの印が
// 残り、次の use_module がその半分の読み込みを読み直さずに使った。指示の実行が脇に置いた問い合わせが失われ、
// 埋め込み側の next() が指示の中へ戻った。どれもゴールの consult/1 と指示の読み込みでは 0.2.2 から同じで、
// フェーズ12 がゴールの use_module/1,2 と ensure_loaded/1 に広げた。上限は 8 MiB。
// 固定の字面はファイルに置かず、ここで tests/tmp/load_throw/ に書く（tests/pl の読み込みと --diff に拾わせない）。
#include "harness.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_limit = std::size_t{1} << 20;

const std::string& fixture_dir() {
  static const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/load_throw";
  return path;
}

std::string fixture(std::string_view name) { return fixture_dir() + "/" + std::string(name); }

void write_file(std::string_view name, std::string_view text) {
  std::ofstream file(fixture(name), std::ios::binary);
  file << text;
}

// heavy.pl の指示は、user:heavy があるときだけヒープの上限まで伸びる。h2/1 はその指示より後の節なので、途中で
// 止まった読み込みには無い。user:heavy は呼ぶ前に assertz で作るので、消した後は黙って失敗する。
void write_fixtures() {
  std::filesystem::create_directories(fixture_dir());
  write_file("gx.pl", ":- module(gx, []).\n");
  write_file("heavy.pl",
             ":- module(heavy, [h1/1, h2/1]).\nh1(1).\n:- (user:heavy -> length(_, 100000000) ; true).\nh2(2).\n");
  write_file("wrap.pl", ":- module(wrap, []).\n:- use_module('heavy.pl').\n");
  const std::string init_goal = ":- initialization((user:heavy -> length(_, 100000000) ; true)).\n";
  write_file("hinit.pl", ":- module(hinit, [hi/1]).\nhi(1).\n" + init_goal + "hi(2).\n");
  write_file("pi_old.pl", "pi(old).\n");
  write_file("pi_new.pl", "pi(new).\n" + init_goal);
  write_file("outer.pl", ":- consult('pi_new.pl').\n");
  write_file("outer_list.pl", ":- ['pi_new.pl'].\n");
  for (const std::string verb : {"use_module", "ensure_loaded", "reexport"}) {
    write_file("wi_" + verb + ".pl", ":- module(wi, []).\n:- " + verb + "('hinit.pl').\n");
  }
  // 連言にくるむと、指示の読み込みではなくゴールの読み込み（load_from_goal）として run_parked の中で動く。
  // ゴールの読み込みは作業ディレクトリから開くので、絶対パスで書く。
  write_file("outer_goal.pl", ":- (true, consult('" + fixture("pi_new.pl") + "')).\n");
  write_file("nest.pl", ":- module(nest, [n/1]).\nn(1).\n:- (true, use_module('" + fixture("heavy.pl") + "')).\nn(2).\n");
}

enum class Outcome { Success, Failure, Other, Limited };

Outcome run(mosa::Machine& machine, const std::string& goal) {
  try {
    const mosa::StopKind kind = machine.query(goal).kind;
    if (kind == mosa::StopKind::Success) return Outcome::Success;
    return kind == mosa::StopKind::Failure ? Outcome::Failure : Outcome::Other;
  } catch (const mosa::ResourceLimit&) {
    return Outcome::Limited;
  }
}

bool succeeds(mosa::Machine& machine, const std::string& goal) { return run(machine, goal) == Outcome::Success; }

bool consult_throws(mosa::Machine& machine, const std::string& path) {
  try {
    (void)machine.consult(path);
  } catch (const mosa::ResourceLimit&) {
    return true;
  }
  return false;
}

// ゴールの読み込みは、呼んだ文脈 gx を load_module にして読む。例外で抜けても user へ戻し、半分のモジュールも残さない。
void check_goal_load(std::string_view verb) {
  const std::string label = std::string(verb) + " goal: ";
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->consult(fixture("gx.pl")), label + "gx loads");
  mosa_test::check(succeeds(*machine, "assertz(user:heavy)."), label + "heavy is set");
  const std::string goal = "gx:" + std::string(verb) + "('" + fixture("heavy.pl") + "').";
  mosa_test::check(run(*machine, goal) == Outcome::Limited, label + "the load stops at the heap limit");
  mosa_test::check(machine->load_program("p(1).\n").ok, label + "a later program loads");
  mosa_test::check(succeeds(*machine, "catch(user:p(1), _, fail)."), label + "the later clause goes to user");
  mosa_test::check(!succeeds(*machine, "current_module(heavy)."), label + "the half-read module is not left");
  mosa_test::check(succeeds(*machine, "retract(user:heavy)."), label + "heavy is cleared");
  mosa_test::check(succeeds(*machine, goal), label + "the load runs again");
  mosa_test::check(succeeds(*machine, "heavy:h2(2)."), label + "the second load reads the whole file");
  mosa_test::check(succeeds(*machine, "gx:h2(2)."), label + "the second load imports into gx");
}

// 指示の use_module が上限で止まると、Machine::consult から例外が抜ける。外の wrap も中の heavy も残さず、
// 読み直せば全部を読む。
void check_directive_load() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(succeeds(*machine, "assertz(user:heavy)."), "directive: heavy is set");
  mosa_test::check(consult_throws(*machine, fixture("wrap.pl")), "directive: consult stops at the heap limit");
  mosa_test::check(!succeeds(*machine, "current_module(heavy)."), "directive: the inner module is not left");
  mosa_test::check(!succeeds(*machine, "current_module(wrap)."), "directive: the outer module is not left");
  mosa_test::check(succeeds(*machine, "retract(user:heavy)."), "directive: heavy is cleared");
  mosa_test::check(machine->consult(fixture("wrap.pl")), "directive: consult runs again");
  mosa_test::check(succeeds(*machine, "heavy:h2(2)."), "directive: the second load reads the whole file");
}

// initialization/1 のゴールはテキストを読み終えた後に動く。そこで上限に当たった一番外の読み込みは、そのゴールが
// Prolog の例外を投げたとき（load_kept）と同じく節と取り込みを残す。load_module は戻す。
void check_init_goal() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->consult(fixture("gx.pl")), "init: gx loads");
  mosa_test::check(succeeds(*machine, "assertz(user:heavy)."), "init: heavy is set");
  const std::string goal = "gx:use_module('" + fixture("hinit.pl") + "').";
  mosa_test::check(run(*machine, goal) == Outcome::Limited, "init: the goal stops at the heap limit");
  mosa_test::check(succeeds(*machine, "hinit:hi(2)."), "init: the read text keeps its clauses");
  mosa_test::check(succeeds(*machine, "predicate_property(gx:hi(_), imported_from(hinit))."), "init: gx keeps imports");
  mosa_test::check(machine->load_program("p(1).\n").ok, "init: a later program loads");
  mosa_test::check(succeeds(*machine, "catch(user:p(1), _, fail)."), "init: the later clause goes to user");
}

// consult、[File]、指示の中のゴールで読んだ子の初期化のゴールは、親のテキストの途中で動くので、例外は親の読みを
// 抜けて親ごと戻す。子が置き換えた pi/1 を戻せるよう、子は例外でも写しを親へ渡す（finish_and_hand_over。
// 渡さないと pi(new) が残る）。
void check_nested_init(std::string_view outer) {
  const std::string label = "nested init (" + std::string(outer) + "): ";
  auto nested = mosa::Machine::create(k_limit);
  mosa_test::check(nested->consult(fixture("pi_old.pl")), label + "pi_old loads");
  mosa_test::check(succeeds(*nested, "assertz(user:heavy)."), label + "heavy is set");
  mosa_test::check(consult_throws(*nested, fixture(outer)), label + "consult stops at the heap limit");
  mosa_test::check(succeeds(*nested, "pi(old)."), label + "the parent puts back the replaced pi/1");
  mosa_test::check(!succeeds(*nested, "catch(pi(new), _, fail)."), label + "the child's pi(new) is gone");
}

// 指示の use_module、ensure_loaded、reexport で読んだファイルは親と同じ走査で読むので、その initialization/1 の
// ゴールは親がテキストを読み終えた後に動く。そこで上限に当たると、一番外の親は子のテキストも読み終えているので、
// 両方を残す。
void check_shared_scan_init(std::string_view verb) {
  const std::string label = "shared init (" + std::string(verb) + "): ";
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(succeeds(*machine, "assertz(user:heavy)."), label + "heavy is set");
  const std::string file = fixture("wi_" + std::string(verb) + ".pl");
  mosa_test::check(consult_throws(*machine, file), label + "consult stops at the heap limit");
  mosa_test::check(succeeds(*machine, "current_module(wi)."), label + "the parent module is kept");
  mosa_test::check(succeeds(*machine, "hinit:hi(2)."), label + "the child keeps its clauses");
  mosa_test::check(succeeds(*machine, "predicate_property(wi:hi(_), imported_from(hinit))."),
                   label + "the parent keeps the child's imports");
}

// 指示の中のゴールの読み込みが上限で止まると、子を戻し、親の指示の run_aside が実行の状態を戻し、run_parked が
// 脇に置いた問い合わせを戻し、親を戻す。wrap.pl は同じ走査の道、outer.pl は consult_one の道なので、run_parked を
// 越える入れ子はここで確かめる。
void check_nested_parked() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(succeeds(*machine, "assertz(user:heavy)."), "nested parked: heavy is set");
  const mosa::Stop first = machine->query("(X = 1 ; X = 2 ; X = 3).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more, "nested parked: the query has a choice point");
  mosa_test::check(consult_throws(*machine, fixture("nest.pl")), "nested parked: consult stops at the heap limit");
  const mosa::Stop second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && mosa::format_answer(*machine, second) == "X = 2",
                   "nested parked: next() gives the query's second answer");
  mosa_test::check(!succeeds(*machine, "current_module(heavy)."), "nested parked: the inner module is not left");
  mosa_test::check(!succeeds(*machine, "current_module(nest)."), "nested parked: the outer module is not left");
  mosa_test::check(succeeds(*machine, "retract(user:heavy)."), "nested parked: heavy is cleared");
  mosa_test::check(machine->consult(fixture("nest.pl")), "nested parked: consult runs again");
  mosa_test::check(succeeds(*machine, "nest:n(2)."), "nested parked: the second load reads the outer file");
  mosa_test::check(succeeds(*machine, "heavy:h2(2)."), "nested parked: the second load reads the inner file");
}

bool load_throws(mosa::Machine& machine, std::string_view text) {
  try {
    (void)machine.load_program(text);
  } catch (const mosa::ResourceLimit&) {
    return true;
  }
  return false;
}

// 指示は、開いた問い合わせの実行を脇に置いて動く（run_parked）。例外で抜けても戻すので、next() は元の問い合わせの
// 次の答えを返す。指示の setup_call_cleanup/3 の後始末は、問い合わせが C++ の例外で抜けるとき（drop_run）と同じく
// 呼ばない。前は残った後始末を次の問い合わせが閉じるときに呼んだ。
void check_parked_query() {
  auto machine = mosa::Machine::create(k_limit);
  const mosa::Stop first = machine->query("(X = 1 ; X = 2 ; X = 3).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more, "parked: the query has a choice point");
  mosa_test::check(load_throws(*machine, ":- length(_, 100000000).\n"), "parked: the directive stops at the limit");
  const mosa::Stop second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && mosa::format_answer(*machine, second) == "X = 2",
                   "parked: next() gives the query's second answer");
  auto other = mosa::Machine::create(k_limit);
  mosa_test::check(other->load_program(":- dynamic(cleaned/0).\n").ok, "cleanup: cleaned is dynamic");
  mosa_test::check(load_throws(*other, ":- setup_call_cleanup(true, length(_, 100000000), assertz(cleaned)).\n"),
                   "cleanup: the directive stops at the limit");
  mosa_test::check(run(*other, "cleaned.") == Outcome::Failure, "cleanup: the directive's cleanup does not run");
}

}  // namespace

int main() {
  write_fixtures();
  check_goal_load("use_module");
  check_goal_load("ensure_loaded");
  check_goal_load("consult");
  check_directive_load();
  check_init_goal();
  check_nested_init("outer.pl");
  check_nested_init("outer_list.pl");
  check_nested_init("outer_goal.pl");
  check_shared_scan_init("use_module");
  check_shared_scan_init("ensure_loaded");
  check_shared_scan_init("reexport");
  check_nested_parked();
  check_parked_query();
  return mosa_test::failures();
}
