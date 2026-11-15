// session_test.cpp の続き（800 行の上限で分けた）。-g の繰り返しと、読み込みだけで終える形と、ゴールの読み込みの
// 失敗の知らせ方を見る。
#include "harness.hpp"
#include "machine/std_capture.hpp"
#include "mosa/machine.hpp"
#include "mosa/repl.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

// レビュー L10。端末でない入力でも -t halt でも、-g が無ければ読み込みの後で 0 で終える（mosac の実行ファイルと同じ）。
// initialization/1 だけのファイルは、読み込みで main を動かした後に使い方を出して 1 で終えていた。
// main が動いたことは、main が作る印のファイルで見る（write の出力は SessionResult に入らない）。
void check_load_then_exit() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_init_only.pl";
  const std::string marker = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_init_ran.txt";
  std::filesystem::remove(marker);
  std::ofstream file(path, std::ios::binary);
  file << ":- initialization(main).\nmain :- open('" << marker << "', write, S), close(S).\n";
  file.close();
  const std::string arg0 = "mosa";
  const std::string arg_t = "-t";
  const std::string halt = "halt";
  std::vector<std::string_view> plain{arg0, path};
  const auto piped = mosa::run_session(plain, "", false);
  mosa_test::check(piped.code == 0 && piped.out.empty() && piped.err.empty(), "init only file exits 0 without tty");
  mosa_test::check(std::filesystem::exists(marker), "init only file runs main");
  std::vector<std::string_view> halted{arg0, path, arg_t, halt};
  const auto batch = mosa::run_session(halted, "", false);
  mosa_test::check(batch.code == 0 && batch.err.empty(), "-t halt without -g loads then exits 0");
  const auto talk = mosa::run_session(halted, "\n", true);
  mosa_test::check(talk.code == 0 && talk.err.empty() && talk.out.find("?- ") == std::string::npos,
                   "-t halt without -g skips the prompt on a tty");
  std::vector<std::string_view> bare{arg0, arg_t, halt};
  const auto nothing = mosa::run_session(bare, "", false);
  mosa_test::check(nothing.code == 0 && nothing.err.empty(), "-t halt alone exits 0");
  std::vector<std::string_view> none{arg0};
  const auto usage = mosa::run_session(none, "", false);
  mosa_test::check(usage.code == 1 && !usage.err.empty(), "no file, goal or -t halt without tty is a usage error");
}

// レビュー L10。-g は繰り返してよく、書いた順に動く。一括では成功しなかった最初のゴールの値で終え、後は動かさない。
mosa::SessionResult run_goals(std::initializer_list<std::string_view> goals, bool tty, bool top_halt) {
  std::vector<std::string_view> args{std::string_view{"mosa"}};
  for (const std::string_view goal : goals) {
    args.emplace_back("-g");
    args.push_back(goal);
  }
  if (top_halt) {
    args.emplace_back("-t");
    args.emplace_back("halt");
  }
  return mosa::run_session(args, "\n", tty);
}

void check_repeated_goals() {
  const auto both = run_goals({"X = 1.", "Y = 2."}, false, true);
  mosa_test::check(both.code == 0 && both.out == "X = 1.\nY = 2.\n", "two -g run in order");
  const auto failed = run_goals({"fail.", "Y = 2."}, false, true);
  mosa_test::check(failed.code == 1 && failed.out == "false.\n", "batch stops at a failing -g");
  const auto later = run_goals({"X = 1.", "fail.", "Y = 2."}, false, true);
  mosa_test::check(later.code == 1 && later.out == "X = 1.\nfalse.\n", "batch stops at a later failing -g");
  const auto raised = run_goals({"throw(oops).", "Y = 2."}, false, true);
  mosa_test::check(raised.code == 2 && raised.out.find("Y = 2.") == std::string::npos, "batch stops at a raising -g");
  const auto broken = run_goals({"foo(", "Y = 2."}, false, true);
  mosa_test::check(broken.code == 2 && broken.out.find("Y = 2.") == std::string::npos, "batch stops at a broken -g");
  const auto halted = run_goals({"halt(5).", "Y = 2."}, false, true);
  mosa_test::check(halted.code == 5 && halted.out.empty(), "halt in the first -g stops the rest");
  const auto piped = run_goals({"X = 1.", "fail.", "Y = 2."}, false, false);
  mosa_test::check(piped.code == 1 && piped.out == "X = 1.\nfalse.\n", "no tty without -t halt is batch too");
  const auto tty_halt = run_goals({"fail.", "Y = 2."}, true, true);
  mosa_test::check(tty_halt.code == 1 && tty_halt.out == "false.\n", "tty with -t halt is batch");
  const auto talk = run_goals({"fail.", "Y = 2."}, true, false);
  mosa_test::check(talk.code == 0 && talk.out.find("false.\nY = 2.\n?- ") != std::string::npos,
                   "tty runs every -g then prompts");
}

// フェーズ12の問い 1 の A。ゴールの consult/1 が失敗したら、その時点で診断を user_error に 1 回だけ出す。;、findall/3、
// \+ が失敗を吸っても出し、問い合わせ全体が失敗したときにトップレベルが同じ行を繰り返さないことを、回数で見る。
namespace {

constexpr std::string_view kMissingLine = "/no/such/p12.pl: ファイルを開けません";

// 有無ではなく回数を数える。find では、出ないことと二重に出ることの片方しか捕まえられない。
std::size_t count_of(std::string_view text, std::string_view line) {
  std::size_t count = 0;
  for (auto at = text.find(line); at != std::string_view::npos; at = text.find(line, at + line.size())) ++count;
  return count;
}

mosa::SessionResult run_goal(std::string_view goal) {
  const std::vector<std::string_view> args{std::string_view{"mosa"}, std::string_view{"-g"}, goal};
  return mosa::run_session(args, "", false);
}

void check_goal_load_report_count() {
  const auto both = run_goal("(\\+ consult('/no/such/p12.pl'), \\+ consult('/no/such/p12.pl')).");
  mosa_test::check(both.code == 0 && both.out.find("true.") != std::string::npos, "swallowed consult failures succeed");
  mosa_test::check(count_of(both.err, kMissingLine) == 2, "each swallowed consult failure prints its line");
}

void check_goal_load_report_once() {
  const auto plain = run_goal("consult('/no/such/p12.pl').");
  mosa_test::check(plain.code == 1, "failed goal consult exits 1");
  mosa_test::check(count_of(plain.err, kMissingLine) == 1, "toplevel does not repeat the goal's line");
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/p12_busy.pl";
  std::ofstream file(path, std::ios::binary);
  file << "p(1).\n";
  file.close();
  const std::string goal = "(consult('" + path + "'), findall(x, consult('" + path + "'), L), L == []).";
  const auto busy = run_goal(goal);
  mosa_test::check(busy.code == 0, "refused reload inside findall succeeds");
  mosa_test::check(count_of(busy.err, path + ": 実行中の述語は置き換えられない") == 1,
                   "refused reload inside findall prints its line once");
}

// 埋め込み（セッションが無い）では std::cerr に書く。harness の FAIL: も std::cerr に出るので、捕まえを閉じてから比べる。
void check_goal_load_report_embedded() {
  mosa::StopKind kind = mosa::StopKind::Failure;
  std::string text;
  {
    const mosa::StdCapture capture;
    auto machine = mosa::Machine::create();
    kind = machine->query("\\+ consult('/no/such/p12.pl').").kind;
    text = capture.text();
  }
  mosa_test::check(kind == mosa::StopKind::Success, "embedded swallowed consult failure succeeds");
  mosa_test::check(count_of(text, kMissingLine) == 1, "embedded consult failure prints to std::cerr once");
}

}  // namespace

void check_goal_load_reports() {
  check_goal_load_report_count();
  check_goal_load_report_once();
  check_goal_load_report_embedded();
}
