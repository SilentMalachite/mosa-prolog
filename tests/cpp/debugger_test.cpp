// フェーズ 14（ADR 0028）。追跡しても答えが変わらないこと（カット、findall/3、catch/3、setup_call_cleanup/3、call/N）、
// 決定的な問い合わせの後に番兵が残らないこと（has_more は箱の選択点を数えないので、; の問いでは見つからない）、
// DebugConsole の無い機械（mosatest、mosac の実行ファイル、埋め込み）は trace の後も箱を作らないこと、
// setup_call_cleanup/3 の選択点を中に残した箱を r と f で切っても後始末が外された回数だけ動くこと（タスク 6）、
// a が StopKind::Abort で問い合わせを止め、後始末を動かさず、デバッグの状態を変えないこと（タスク 7）。
// ポートの行そのものはトランスクリプト（tests/cpp/data/debugger）で確かめる。
#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"

#include <array>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view k_program =
    ":- use_module(library(lists)).\n"
    "len([], 0).\n"
    "len([_|T], N) :- len(T, M), N is M + 1.\n";

// 書いた行を覚える。読む行はいつも空で、どのポートでも creep になる。
struct Console {
  std::string text;
  std::size_t writes = 0;
};

std::unique_ptr<mosa::Machine> make_machine() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, "load len/2 and library(lists)");
  // 行の字面を LANG に左右させない。
  mosa::engine_impl(*machine).message_lang = mosa::MessageLanguage::En;
  return machine;
}

void give_console(mosa::Machine& machine, Console& console) {
  mosa::DebugConsole made;
  made.write = [&console](std::string_view text) {
    console.text += text;
    ++console.writes;
  };
  made.read_line = [](std::string& line) {
    line = "\n";
    return true;
  };
  mosa::engine_impl(machine).debug.console = std::move(made);
}

std::string stop_text(const mosa::Machine& machine, const mosa::Stop& stop) {
  if (stop.kind == mosa::StopKind::Success) return mosa::format_answer(machine, stop) + (stop.more ? " ;" : " .");
  if (stop.kind == mosa::StopKind::Failure) return "false";
  if (stop.kind == mosa::StopKind::Abort) return "abort";
  return stop.kind == mosa::StopKind::Exception ? "exception" : "halt";
}

// 答えを尽きるまで順に字面にする。上限は、答えが尽きない誤りでテストが止まらないため。
std::vector<std::string> all_answers(mosa::Machine& machine, const std::string& goal) {
  std::vector<std::string> texts;
  mosa::Stop stop = machine.query(goal + ".");
  for (int count = 0; count < 8; ++count) {
    texts.push_back(stop_text(machine, stop));
    if (stop.kind != mosa::StopKind::Success || !stop.more) break;
    stop = machine.next();
  }
  return texts;
}

std::string joined(const std::vector<std::string>& texts) {
  std::string out;
  for (const std::string& text : texts) out += "[" + text + "]";
  return out;
}

// Review Focus 1。creep で最後まで追った答えと、デバッグなしの答えが、答えの数と ; の有無まで同じ。
void check_trace_same_answers() {
  constexpr std::array<std::string_view, 9> goals{
      "findall(X, (member(X, [1,2,3]), X > 1), L)",
      "once(member(X, [a,b]))",
      "(member(X, [1,2]) -> Y = X ; Y = none)",
      "(member(X, [1,2]) *-> true ; X = none)",
      "\\+ member(z, [a])",
      "catch((member(X, [1,2]), X > 1, throw(t(X))), t(Y), true)",
      "setup_call_cleanup(true, member(X, [1,2]), true), X > 1",
      "call(len, [a,b], N)",
      "(member(X, [1,2,3]), X >= 2, !)",
  };
  for (const std::string_view goal : goals) {
    auto plain = make_machine();
    auto traced = make_machine();
    Console console;
    give_console(*traced, console);
    const std::string label{goal};
    const auto expected = all_answers(*plain, label);
    const auto actual = all_answers(*traced, "trace, " + label);
    mosa_test::check(expected == actual, label + ": same answers, plain " + joined(expected) + " traced " + joined(actual));
    mosa_test::check(console.writes > 0, label + ": ports were written");
  }
}

// 決定的な問い合わせを追った後、番兵も DebugRedo も残らない。catch/3 と *-> の中で抜ける箱も見る。どちらも上に
// 選択点が無いかを stack.size() で決める（disarm_catch、SoftCommit）ので、番兵が残れば選択点も残る。
void check_trace_choice_depth() {
  constexpr std::array<std::string_view, 3> goals{
      "len([a,b], N)",
      "catch(len([a],N), _, true)",
      "(len([a],N) *-> true ; true)",
  };
  for (const std::string_view goal : goals) {
    auto plain = make_machine();
    auto traced = make_machine();
    Console console;
    give_console(*traced, console);
    const std::string label{goal};
    const mosa::Stop plain_stop = plain->query(label + ".");
    const mosa::Stop traced_stop = traced->query("trace, " + label + ".");
    mosa_test::check(plain_stop.kind == mosa::StopKind::Success && traced_stop.kind == mosa::StopKind::Success,
                     label + ": both succeed");
    mosa_test::check(plain->choice_depth() == traced->choice_depth(),
                     label + ": depth " + std::to_string(plain->choice_depth()) + " plain, " +
                         std::to_string(traced->choice_depth()) + " traced");
    mosa_test::check(console.writes > 0, label + ": ports were written");
    if (goal == goals[0]) {
      mosa_test::check(console.text.find(" Exit: (1) len([a,b],2) ? ") != std::string::npos,
                       label + ": the exit port of the outer box, got [" + console.text + "]");
    }
  }
}

// 口が無ければ、trace の後もポートの行はどこにも出ず、箱の番号も 1 つも使わない。
void check_no_console() {
  auto plain = make_machine();
  auto bare = make_machine();
  const mosa::Stop plain_stop = plain->query("len([a], N).");
  std::ostringstream out;
  std::ostringstream err;
  std::streambuf* saved_out = std::cout.rdbuf(out.rdbuf());
  std::streambuf* saved_err = std::cerr.rdbuf(err.rdbuf());
  const mosa::Stop stop = bare->query("trace, len([a], N).");
  std::cout.rdbuf(saved_out);
  std::cerr.rdbuf(saved_err);
  const auto& impl = mosa::engine_impl(*bare);
  mosa_test::check(stop.kind == mosa::StopKind::Success && plain_stop.kind == mosa::StopKind::Success,
                   "no console: succeeds");
  mosa_test::check(impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep, "no console: trace still sets the state");
  mosa_test::check(out.str().empty() && err.str().empty(), "no console: nothing written, got [" + err.str() + "]");
  mosa_test::check(impl.debug.next_box == 1, "no console: no box was opened");
  mosa_test::check(bare->choice_depth() == plain->choice_depth(), "no console: same choice depth");
}

constexpr std::string_view k_cleanup_program =
    ":- use_module(library(lists)).\n"
    ":- dynamic(cleaned/0).\n"
    "r :- setup_call_cleanup(true, member(_, [1,2]), assertz(cleaned)).\n"
    "s :- setup_call_cleanup(true, member(_, [1,2]), assertz(cleaned)).\n";

// 直前に書かれた行が target のときに 1 回だけ command を返し、ほかは空の行を返す。2 回目からも返すと、再試行が
// 同じポートに戻るたびに同じコマンドを読んで終わらない。
struct Scripted {
  std::string target;
  std::string command;
  std::string last;
  std::string text;
  bool used = false;
};

void give_script(mosa::Machine& machine, Scripted& script) {
  mosa::DebugConsole made;
  made.write = [&script](std::string_view text) {
    script.last = std::string{text};
    script.text += text;
  };
  made.read_line = [&script](std::string& line) {
    line.clear();
    if (!script.used && script.last == script.target) {
      script.used = true;
      line = script.command;
    }
    return true;
  };
  mosa::engine_impl(machine).debug.console = std::move(made);
}

std::size_t occurrences(const std::string& text, std::string_view part) {
  std::size_t count = 0;
  for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++count;
  return count;
}

// 後始末の回数。この問い合わせは前の問い合わせを閉じるので、閉じたときの後始末も数に入る。
std::string cleanup_count(mosa::Machine& machine) {
  const mosa::Stop stop = machine.query("nodebug, findall(x, cleaned, L), length(L, N).");
  return stop_text(machine, stop);
}

struct CleanupCase {
  std::string_view label;
  std::string_view goal;
  std::string_view target;
  std::string_view command;
};

// Review Focus 4。setup_call_cleanup/3 の選択点を中に残した箱を r か f で切っても、後始末は外された回数だけ動く。
std::string run_cleanup_case(const CleanupCase& item, std::string& answer, std::string& ports) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_cleanup_program).ok, std::string{item.label} + ": load the program");
  mosa::engine_impl(*machine).message_lang = mosa::MessageLanguage::En;
  Scripted script{std::string{item.target}, std::string{item.command}, {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query(std::string{item.goal} + ".");
  answer = stop_text(*machine, stop);
  const std::string count = cleanup_count(*machine);
  mosa_test::check(script.used, std::string{item.label} + ": the command was read, ports [" + script.text + "]");
  ports = script.text;
  return count;
}

// 成功のポートで r。再試行で番兵より上を切るので 1 回、閉じて 1 回。
void check_retry_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count = run_cleanup_case({"retry_cleanup", "trace, r", " Exit: (1) r ? ", "r"}, answer, ports);
  mosa_test::check(count.find("N = 2") != std::string::npos, "retry_cleanup: N = 2, got [" + count + "]");
  mosa_test::check(answer.ends_with(" ;"), "retry_cleanup: answer with more, got [" + answer + "]");
  mosa_test::check(occurrences(ports, " Call: (1) r ? ") == 2, "retry_cleanup: the box was reopened, [" + ports + "]");
}

// 最初の再試行のポートで r。切り詰めで 1 回、2 回目の s が member を使い切って 1 回。
void check_redo_retry_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count =
      run_cleanup_case({"redo_retry_cleanup", "trace, s, fail", " Redo: (1) s ? ", "r"}, answer, ports);
  mosa_test::check(count.find("N = 2") != std::string::npos, "redo_retry_cleanup: N = 2, got [" + count + "]");
  mosa_test::check(answer == "false", "redo_retry_cleanup: false, got [" + answer + "]");
}

// 最初の再試行のポートで f。切り詰めで 1 回だけ。f を creep と読むと 2 つ目の答えの成功のポートが出るので、
// s の成功のポートが 1 回だけであることも見る。
void check_redo_fail_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count =
      run_cleanup_case({"redo_fail_cleanup", "trace, s, fail", " Redo: (1) s ? ", "f"}, answer, ports);
  mosa_test::check(count.find("N = 1") != std::string::npos, "redo_fail_cleanup: N = 1, got [" + count + "]");
  mosa_test::check(answer == "false", "redo_fail_cleanup: false, got [" + answer + "]");
  mosa_test::check(occurrences(ports, " Exit: (1) s ? ") == 1, "redo_fail_cleanup: one exit of s, [" + ports + "]");
  mosa_test::check(occurrences(ports, " Fail: (1) s ? ") == 1, "redo_fail_cleanup: one fail of s, [" + ports + "]");
}

// タスク 7。呼出のポートの a で問い合わせは Abort で止まり、デバッグの状態は変えない。印は次の問い合わせの
// drop_run が消す。その前の load_program の指令は、残った印で止まらない（HaltSetAside が脇に置く）。
void check_abort_stop() {
  auto machine = make_machine();
  Scripted script{" Call: (2) len([],_G1) ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, len([a], N).");
  const auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(stop.kind == mosa::StopKind::Abort, "abort_stop: Abort, got [" + stop_text(*machine, stop) + "]");
  mosa_test::check(impl.abort_now, "abort_stop: abort_now is set until the next query");
  mosa_test::check(impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep, "abort_stop: the debug state is kept");
  mosa_test::check(occurrences(script.text, " Exit: ") == 0, "abort_stop: no port after a, [" + script.text + "]");
  mosa_test::check(machine->load_program(":- true.\n").ok, "abort_stop: a directive after the abort runs");
  mosa_test::check(impl.abort_now, "abort_stop: load_program keeps the mark of the aborted query");
  const mosa::Stop next = machine->query("true.");
  mosa_test::check(next.kind == mosa::StopKind::Success, "abort_stop: the next query succeeds");
  mosa_test::check(!impl.abort_now, "abort_stop: the next query cleared abort_now");
}

// abort は halt と同じく、setup_call_cleanup/3 の後始末を動かさない。次の問い合わせが前を閉じるときも動かさない。
void check_abort_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count = run_cleanup_case({"abort_cleanup", "trace, r", " Exit: (1) r ? ", "a"}, answer, ports);
  mosa_test::check(answer == "abort", "abort_cleanup: Abort, got [" + answer + "]");
  mosa_test::check(count.find("N = 0") != std::string::npos, "abort_cleanup: N = 0, got [" + count + "]");
}

// initialization/1 のゴールの abort。例外と同じく読み込みは失敗し、後の初期化は動かない。診断は書かない。節は
// 例外のときと同じく残る（2026-10-26 の決定 A。設計の「元に戻す」との食い違いは open-items の問い）。
void check_abort_init() {
  auto machine = make_machine();
  Scripted script{" Call: (1) dbg_init ? ", "a", {}, {}, false};
  give_script(*machine, script);
  mosa_test::check(machine->query("trace.").kind == mosa::StopKind::Success, "abort_init: trace");
  const mosa::LoadStatus status = machine->load_program(
      "dbg_init.\nloaded(1).\n:- initialization(dbg_init).\n:- initialization(assertz(second_init)).\n");
  const auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(!status.ok && status.message.empty(),
                   "abort_init: fails with no diagnostic, [" + status.message + "]");
  mosa_test::check(impl.abort_now, "abort_init: abort_now is kept for the caller");
  const mosa::Stop kept =
      machine->query("notrace, nodebug, current_predicate(loaded/1), \\+ current_predicate(second_init/0).");
  mosa_test::check(kept.kind == mosa::StopKind::Success,
                   "abort_init: clauses kept, no later init, got [" + stop_text(*machine, kept) + "]");
}

// コントローラの裁定 16。skip のまま捨てた問い合わせの skip_box はもうポートを出さない。次の問い合わせの drop_run
// が creep に戻さないと、次の trace まで箱が 1 つも作られない。
void check_skip_dropped() {
  auto machine = make_machine();
  auto& impl = mosa::engine_impl(*machine);
  impl.debug_on = true;
  impl.debug.mode = mosa::DebugMode::Skip;
  impl.debug.skip_box = 7;
  mosa_test::check(machine->query("true.").kind == mosa::StopKind::Success, "skip_dropped: the query succeeds");
  mosa_test::check(impl.debug.mode == mosa::DebugMode::Creep && impl.debug.skip_box == 0,
                   "skip_dropped: drop_run returns skip to creep");
}

}  // namespace

int main() {
  check_trace_same_answers();
  check_trace_choice_depth();
  check_no_console();
  check_retry_cleanup();
  check_redo_retry_cleanup();
  check_redo_fail_cleanup();
  check_abort_stop();
  check_abort_cleanup();
  check_abort_init();
  check_skip_dropped();
  return mosa_test::failures() == 0 ? 0 : 1;
}
