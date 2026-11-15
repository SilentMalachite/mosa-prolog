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
  bool threw = false;
  try {
    (void)machine->consult(fixture("wrap.pl"));
  } catch (const mosa::ResourceLimit&) {
    threw = true;
  }
  mosa_test::check(threw, "directive: consult stops at the heap limit");
  mosa_test::check(!succeeds(*machine, "current_module(heavy)."), "directive: the inner module is not left");
  mosa_test::check(!succeeds(*machine, "current_module(wrap)."), "directive: the outer module is not left");
  mosa_test::check(succeeds(*machine, "retract(user:heavy)."), "directive: heavy is cleared");
  mosa_test::check(machine->consult(fixture("wrap.pl")), "directive: consult runs again");
  mosa_test::check(succeeds(*machine, "heavy:h2(2)."), "directive: the second load reads the whole file");
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
  check_parked_query();
  return mosa_test::failures();
}
