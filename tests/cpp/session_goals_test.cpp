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
  // text は標準出力と標準エラーを流れの名前付きで繋ぐので、全体を比べて、標準出力が空で標準エラーがその 1 行だけで
  // あることを見る。数えるだけだと、std::cout に書き違えても通る。
  mosa_test::check(text == "\nstderr: " + std::string{kMissingLine} + "\n",
                   "embedded consult failure prints once to std::cerr only: [" + text + "]");
}

// フェーズ12 Task 3b。指令の中で吸われたゴールの読み込みの失敗は、その行を 1 回出すだけで、後の指令と外側の読み込みを
// 落とさない。前は残った診断の字面を、次の一般の指令が op/3 の誤りと取り違え、外側の読み込みごと失敗させていた。
constexpr std::string_view kSwallowed = ":- (consult('/no/such/p12.pl') -> true ; true).\n";

std::string write_fixture(std::string_view name, std::string_view body) {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/" + std::string{name};
  std::ofstream file(path, std::ios::binary);
  file << body;
  return path;
}

// goal が空なら -g を付けず、読み込みだけで終える（読み込みの失敗は 1）。
mosa::SessionResult run_file(const std::string& path, std::string_view goal) {
  std::vector<std::string_view> args{std::string_view{"mosa"}, path};
  if (!goal.empty()) {
    args.emplace_back("-g");
    args.push_back(goal);
  }
  return mosa::run_session(args, "", false);
}

// 期待する行はマシンに作らせて、err を丸ごと比べる。文面は LANG で変わるが、同じプロセスのマシンは同じ言語で書くので、
// 言語に依らない。丸ごと比べれば行の数も決まり、外側のパスを付けた 2 行目や重複も落ちる。
std::string consult_line(const std::string& path) {
  auto machine = mosa::Machine::create();
  const bool loaded = machine->consult(path);
  return loaded ? std::string{"<loaded>"} : std::string{machine->diagnostic()};
}

std::string load_reason(std::string_view program) {
  const mosa::LoadStatus status = mosa::Machine::create()->load_program(program);
  return status.ok ? std::string{"<loaded>"} : status.message;
}

void check_goal_load_recovered_in_directive() {
  const std::string path = write_fixture("p12_recover.pl", std::string{kSwallowed} + ":- X = 1, X == 1.\np12_r(1).\n");
  const auto result = run_file(path, "p12_r(X).");
  mosa_test::check(result.code == 0 && result.out == "X = 1.\n",
                   "swallowed consult failure keeps later directives and the outer load: [" + result.out + "]");
  mosa_test::check(result.err == consult_line("/no/such/p12.pl") + "\n",
                   "swallowed consult failure prints only its own line once: [" + result.err + "]");
}

// write の出力は SessionResult に入らない（check_load_then_exit）ので、演算子は答えの行で見る。
void check_goal_load_recovered_then_op() {
  const std::string path = write_fixture("p12_recover_op.pl", std::string{kSwallowed} + ":- op(700, xfx, ===>).\n");
  const auto result = run_file(path, "X = (a ===> b).");
  mosa_test::check(result.code == 0 && result.out == "X = (a===>b).\n",
                   "op/3 after a swallowed consult failure is defined: [" + result.out + "]");
  mosa_test::check(result.err == consult_line("/no/such/p12.pl") + "\n",
                   "op/3 after a swallowed consult failure prints only the consult line: [" + result.err + "]");
}

// apply_op が断る形（優先度の範囲外）と受けない形（優先度が数でない）。吸われた失敗の後でも、単独のときと同じ文で
// 外側の読み込みを落とす。受けない形は前、残った consult の字面を外側の診断にしていた。
void check_bad_op_still_fails() {
  for (const std::string_view directive : {":- op(1201, xfx, foo).\n", ":- op(foo, xfx, bar).\n"}) {
    const std::string reason = load_reason(directive);
    const std::string alone = write_fixture("p12_bad_op.pl", directive);
    const auto plain = run_file(alone, "");
    mosa_test::check(plain.code == 1 && plain.err == alone + ": " + reason + "\n",
                     "bad op alone fails the load: [" + plain.err + "]");
    const std::string after = write_fixture("p12_recover_bad_op.pl", std::string{kSwallowed} + std::string{directive});
    const auto late = run_file(after, "");
    mosa_test::check(late.code == 1 && late.err == consult_line("/no/such/p12.pl") + "\n" + after + ": " + reason + "\n",
                     "bad op after a swallowed consult failure keeps its own diagnostic: [" + late.err + "]");
  }
}

// 吸われたゴールの consult が構文エラーで失敗すると、行と桁を付ける印が残る。後の set_prolog_flag/2 の指令の誤りが
// その印を引き継ぐと、外側のパスに中のファイルの行と桁が付く。単独のときと同じ「パス: 理由」の 1 行であることを見る。
void check_goal_load_recovered_then_flag() {
  const std::string broken = write_fixture("p12_broken.pl", "p(.\n");
  const std::string path = write_fixture("p12_recover_flag.pl", ":- (consult('" + broken + "') -> true ; true).\n"
                                                                ":- set_prolog_flag(bogus, 1).\n");
  const auto result = run_file(path, "");
  const std::string reason = load_reason(":- set_prolog_flag(bogus, 1).\n");
  mosa_test::check(result.code == 1 && result.err == consult_line(broken) + "\n" + path + ": " + reason + "\n",
                   "bad flag after a swallowed syntax error has no stale line and column: [" + result.err + "]");
}

// フェーズ12の最終レビュー Important 1。ゴールで読んだファイルが halt/0,1 で止めたのは読み込みの失敗ではないので、理由の
// 行を出さない。前は空の理由にパスを付けた「F: 」の行を出していた（c842021 も指令の :- use_module(F). も出さない）。
// ファイルが書く hi は std::cout に出て SessionResult に入らないので、捕まえて全体を比べる。std::cerr に出た行も落ちる。
struct HaltedRun {
  mosa::SessionResult result;
  std::string captured;
};

HaltedRun run_halting(const std::string& goal) {
  HaltedRun run;
  const mosa::StdCapture capture;
  run.result = run_goal(goal);
  run.captured = capture.text();
  return run;
}

// 終了コードは前から halt の値なので、赤くなるのは行の検査だけ。分けて、どちらが落ちたかを見せる。
void check_halting_goal(const std::string& goal, int code, const std::string& label) {
  const HaltedRun run = run_halting(goal);
  mosa_test::check(run.result.code == code && run.result.out.empty(),
                   label + " exits with the halt value: [" + std::to_string(run.result.code) + "][" + run.result.out + "]");
  mosa_test::check(run.result.err.empty(), label + " writes no reason line: [" + run.result.err + "]");
  mosa_test::check(run.captured == "\nstdout: hi\n", label + " keeps only the file's own output: [" + run.captured + "]");
}

// 埋め込み（セッションが無い）では理由の行は std::cerr に出る。mosac の実行ファイルも同じ書き先を通る。
void check_halting_goal_embedded(const std::string& path) {
  mosa::Stop stop;
  std::string text;
  {
    const mosa::StdCapture capture;
    auto machine = mosa::Machine::create();
    stop = machine->query("consult('" + path + "').");
    text = capture.text();
  }
  mosa_test::check(stop.kind == mosa::StopKind::Halt && stop.halt_code == 4, "embedded goal consult of a halting file halts");
  mosa_test::check(text == "\nstdout: hi\n", "embedded goal consult of a halting file writes no reason line: [" + text + "]");
}

// フェーズ12のバグの作業（最終レビューの台帳 L74）。埋め込みの入口で halt/0,1 で止まった読み込みは、失敗ではなく終わり
// なので、diagnostic() を空にする（セッションの consult_files と実行ファイルの load_outcome は halt_now を先に見る）。
// 前は「F: 」や、吸われた失敗の字面に外側のパスを付けた物が残った。捕まえを閉じてから比べる（harness の FAIL: も std::cerr）。
struct EmbeddedLoad {
  bool ok = true;
  std::string diagnostic;
  std::string captured;
};

EmbeddedLoad consult_embedded(const std::string& path) {
  EmbeddedLoad load;
  const mosa::StdCapture capture;
  auto machine = mosa::Machine::create();
  load.ok = machine->consult(path);
  load.diagnostic = std::string{machine->diagnostic()};
  load.captured = capture.text();
  return load;
}

void check_embedded_consult_halts(const std::string& plain, const std::string& swallowed) {
  const EmbeddedLoad halted = consult_embedded(plain);
  mosa_test::check(!halted.ok, "embedded consult of a halting file returns false");
  mosa_test::check(halted.diagnostic.empty(), "embedded consult of a halting file leaves no diagnostic: [" +
                                                  halted.diagnostic + "]");
  mosa_test::check(halted.captured == "\nstdout: hi\n", "embedded consult of a halting file: [" + halted.captured + "]");
  const EmbeddedLoad after = consult_embedded(swallowed);
  mosa_test::check(!after.ok && after.diagnostic.empty(),
                   "embedded consult that halts after a swallowed failure leaves no diagnostic: [" + after.diagnostic + "]");
  mosa_test::check(after.captured == "\nstderr: " + consult_line("/no/such/p12.pl") + "\n",
                   "the swallowed failure still prints once: [" + after.captured + "]");
}

// 問い合わせの入口と load_program も同じ。halt の後の読み込みは halt_now が残っていても、次の読み込みの診断を正しく出す。
void check_embedded_entries_halt(const std::string& plain, const std::string& module, const std::string& syntax) {
  mosa::Stop consulted;
  mosa::Stop used;
  mosa::LoadStatus program;
  mosa::LoadStatus bad_program;
  bool broken = true;
  std::vector<std::string> seen;
  std::string text;
  {
    const mosa::StdCapture capture;
    auto machine = mosa::Machine::create();
    consulted = machine->query("consult('" + plain + "').");
    seen.emplace_back(machine->diagnostic());
    used = machine->query("use_module('" + module + "').");
    seen.emplace_back(machine->diagnostic());
    program = machine->load_program(std::string{kSwallowed} + ":- halt(5).\n");
    seen.emplace_back(machine->diagnostic());
    broken = machine->consult(syntax);
    seen.emplace_back(machine->diagnostic());
    (void)machine->query("halt.");
    bad_program = machine->load_program("r :- .\n");
    text = capture.text();
  }
  mosa_test::check(consulted.kind == mosa::StopKind::Halt && seen[0].empty(),
                   "goal consult that halts leaves no diagnostic: [" + seen[0] + "]");
  mosa_test::check(used.kind == mosa::StopKind::Halt && seen[1].empty(),
                   "goal use_module that halts leaves no diagnostic: [" + seen[1] + "]");
  mosa_test::check(!program.ok && program.message.empty() && seen[2].empty(),
                   "load_program that halts after a swallowed failure: [" + program.message + "][" + seen[2] + "]");
  mosa_test::check(!broken && seen[3].starts_with(syntax + ":1:"),
                   "a consult after a halted load reports its own syntax error: [" + seen[3] + "][" + text + "]");
  mosa_test::check(!bad_program.ok && !bad_program.message.empty(),
                   "a load_program after a halted query reports its own syntax error: [" + text + "]");
}

// :- halt(N). と initialization の halt は run_load の別の道で抜けるので、両方を通す。halt/0 は 0 で終え、答えも出さない。
void check_goal_load_halts() {
  const std::string plain = write_fixture("p12_halt_c.pl", ":- write(hi), nl.\n:- halt(4).\n");
  check_halting_goal("consult('" + plain + "').", 4, "goal consult of a file that halts");
  const std::string module =
      write_fixture("p12_halt_m.pl", ":- module(p12_halt_m, []).\n:- initialization((write(hi), nl, halt(3))).\n");
  check_halting_goal("use_module('" + module + "').", 3, "goal use_module/1 of a file whose initialization halts");
  check_halting_goal("use_module('" + module + "', []).", 3, "goal use_module/2 of a file whose initialization halts");
  const std::string ensured = write_fixture("p12_halt_e.pl", ":- write(hi), nl.\n:- halt.\n");
  check_halting_goal("ensure_loaded('" + ensured + "').", 0, "goal ensure_loaded of a file that halts");
  check_halting_goal_embedded(plain);
  // 吸われた失敗の字面が残ったまま halt しても、それに外側のパスを付けた 2 行目を出さない（台帳 L74 のセッションの側）。
  const std::string swallowed = write_fixture("p12_halt_swallow.pl", std::string{kSwallowed} + ":- halt(3).\n");
  const auto result = run_goal("consult('" + swallowed + "').");
  mosa_test::check(result.code == 3 && result.err == consult_line("/no/such/p12.pl") + "\n",
                   "halt after a swallowed consult failure prints only the swallowed line: [" + result.err + "]");
  check_embedded_consult_halts(plain, swallowed);
  check_embedded_entries_halt(plain, module, write_fixture("p12_halt_syntax.pl", "r :- .\n"));
}

// フェーズ12 Task 4。use_module/1,2 と ensure_loaded/1 のゴール。プロンプトで読めることと、失敗の診断の字面（計画の
// 「設計との違い」6）が 1 回だけ出ることを見る。
void check_goal_use_at_prompt() {
  const std::vector<std::string_view> args{std::string_view{"mosa"}};
  const auto result = mosa::run_session(args, "use_module(library(lists)).\nlast([1, 2], X).\n", true);
  mosa_test::check(result.out.find("X = 2") != std::string::npos,
                   "use_module at the prompt loads lists: [" + result.out + "][" + result.err + "]");
}

// prefix で始まる行の数。構文の誤りの桁は読み手の数え方に依るので、行の頭だけを見る。
std::size_t lines_starting(std::string_view text, std::string_view prefix) {
  std::size_t count = 0;
  for (std::size_t at = 0; at < text.size();) {
    const auto end = text.find('\n', at);
    const auto line = text.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
    if (line.starts_with(prefix)) ++count;
    if (end == std::string_view::npos) break;
    at = end + 1;
  }
  return count;
}

void check_goal_use_reports_file() {
  const std::string failing = write_fixture("p12_fail.pl", ":- module(p12_fail, []).\n:- fail.\n");
  const auto failed = run_goal("use_module('" + failing + "').");
  // 開いたファイルの中の誤りは「F: 理由」（ja では「F: この指示のゴールは失敗した: :-fail」）。理由は同じプロセスの
  // マシンに作らせ、言語に依らない。
  const std::string reason = load_reason(":- fail.\n");
  mosa_test::check(failed.code == 1 && count_of(failed.err, failing + ": " + reason + "\n") == 1,
                   "failing directive in a goal use_module prints its line once: [" + failed.err + "]");
  const std::string broken = write_fixture("p12_syntax.pl", ":- module(p12_syntax, []).\nr :- .\n");
  const auto syntax = run_goal("use_module('" + broken + "').");
  mosa_test::check(syntax.code == 1 && lines_starting(syntax.err, broken + ":2:") == 1,
                   "syntax error in a goal use_module prints path:line once: [" + syntax.err + "]");
  const auto library = run_goal("use_module(library(p12_none)).");
  mosa_test::check(library.code == 1 &&
                       count_of(library.err, "library(p12_none): existence_error(source_sink,library(p12_none))") == 1,
                   "missing library in a goal use_module prints the spec once: [" + library.err + "]");
}

void check_goal_ensure_busy() {
  const std::string first = write_fixture("p12_busy_a.pl", "p12_p(1).\n");
  const std::string second = write_fixture("p12_busy_b.pl", "p12_p(2).\n");
  const auto result = run_file(first, "((true ; fail), ensure_loaded('" + second + "')).");
  // 文面は ctest が LANG=ja_JP.UTF-8 に固定するのに頼る（tests/tests.cmake の ENVIRONMENT_MODIFICATION）。
  mosa_test::check(result.code == 1 && count_of(result.err, second + ": 実行中の述語は置き換えられない") == 1,
                   "refused ensure_loaded under a choice point prints its line once: [" + result.err + "]");
}

}  // namespace

void check_goal_use_session() {
  check_goal_use_at_prompt();
  check_goal_use_reports_file();
  check_goal_ensure_busy();
}

void check_goal_load_reports() {
  check_goal_load_report_count();
  check_goal_load_report_once();
  check_goal_load_report_embedded();
  check_goal_load_recovered_in_directive();
  check_goal_load_recovered_then_op();
  check_bad_op_still_fails();
  check_goal_load_recovered_then_flag();
  check_goal_load_halts();
}
