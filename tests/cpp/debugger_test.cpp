// フェーズ 14（ADR 0028）。追跡しても答えが変わらないこと（カット、findall/3、catch/3、setup_call_cleanup/3、call/N）、
// 決定的な問い合わせの後に番兵が残らないこと（has_more は箱の選択点を数えないので、; の問いでは見つからない）、
// DebugConsole の無い機械（mosatest、mosac の実行ファイル、埋め込み）は trace の後も箱を作らないこと、
// setup_call_cleanup/3 の選択点を中に残した箱を r と f で切っても後始末が外された回数だけ動くこと（タスク 6）、
// a が StopKind::Abort で問い合わせを止め、デバッグの状態を変えないこと（タスク 7）、a が止める実行ごとに内側から
// 後始末を 1 回ずつ動かすこと（2026-11-18 の持ち主の決定 A）と、その動かし方（ポート無し、誤りを捨てる、デバッガの
// 状態を変えない。open-items の既定 A）、
// 定義が見える前に置いたスパイポイントが、後で読み込んだ述語の呼び出しで止まること（タスク 8）、利用者が Lib:G と
// 書いたライブラリの呼び出しが 1 つの箱になること（ADR 0028 の改訂）。
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
    "s :- setup_call_cleanup(true, member(_, [1,2]), assertz(cleaned)).\n"
    "e :- setup_call_cleanup(true, (member(_, [1,2]), throw(oops)), assertz(cleaned)).\n";

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

// 例外のポートで f。巻き戻しで後始末が 1 回動き、失敗に変えても二度目は動かない。例外は失敗になるので
// catch/3 には届かず、問い合わせは失敗する。
void check_exception_fail_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count = run_cleanup_case(
      {"exception_fail_cleanup", "trace, catch(e, _, true)", " Exception: (1) e oops ? ", "f"}, answer, ports);
  mosa_test::check(count.find("N = 1") != std::string::npos, "exception_fail_cleanup: N = 1, got [" + count + "]");
  mosa_test::check(answer == "false", "exception_fail_cleanup: false, got [" + answer + "]");
  mosa_test::check(occurrences(ports, " Exception: (1) e oops ? ") == 1,
                   "exception_fail_cleanup: one exception of e, [" + ports + "]");
}

// 例外のポートで r。1 回目の巻き戻しの後始末と、やり直してまた投げたときの後始末で 2 回。二度目の例外は
// catch/3 が受ける。
void check_exception_retry_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count = run_cleanup_case(
      {"exception_retry_cleanup", "trace, catch(e, _, true)", " Exception: (1) e oops ? ", "r"}, answer, ports);
  mosa_test::check(count.find("N = 2") != std::string::npos, "exception_retry_cleanup: N = 2, got [" + count + "]");
  mosa_test::check(answer != "false", "exception_retry_cleanup: caught, got [" + answer + "]");
  mosa_test::check(occurrences(ports, " Call: (1) e ? ") == 2,
                   "exception_retry_cleanup: the box was reopened, [" + ports + "]");
  mosa_test::check(occurrences(ports, " Exception: (1) e oops ? ") == 2,
                   "exception_retry_cleanup: two exceptions of e, [" + ports + "]");
}

// library(Name) で読んだモジュールを、利用者のテキストが同じ名前で宣言し直したら、ライブラリの印は戻る。
// 印が残ると、再定義した側の中の呼び出しまでトレースから隠れる。
void check_library_redefine() {
  auto machine = make_machine();
  const auto& impl = mosa::engine_impl(*machine);
  const auto is_library = [&impl](std::string_view name) {
    for (const mosa::Module& module : impl.modules) {
      if (impl.atoms.name(module.name) == name) return module.library;
    }
    return false;
  };
  mosa_test::check(is_library("lists"), "library_redefine: library(lists) is marked");
  mosa_test::check(machine->load_program(":- module(lists, [mine/1]).\nmine(a).\n").ok,
                   "library_redefine: the user text redeclares lists");
  mosa_test::check(!is_library("lists"), "library_redefine: the mark is gone after the redeclaration");
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

// abort は halt と違い、止まる前に setup_call_cleanup/3 の後始末を切って動かす（2026-11-18 の持ち主の決定 A）。後始末は
// ポートを出さない。次の問い合わせが前を閉じるときにはもう残っていないので、数は 1 回になる。
void check_abort_cleanup() {
  std::string answer;
  std::string ports;
  const std::string count = run_cleanup_case({"abort_cleanup", "trace, r", " Exit: (1) r ? ", "a"}, answer, ports);
  mosa_test::check(answer == "abort", "abort_cleanup: Abort, got [" + answer + "]");
  mosa_test::check(count.find("N = 1") != std::string::npos, "abort_cleanup: N = 1, got [" + count + "]");
  // ポートの行は「 Call: (N) ゴール」。上の箱のゴールの字面にも assertz が入るので、行の形で数える。
  mosa_test::check(occurrences(ports, ") assertz(") == 0, "abort_cleanup: no port in the cleanup, [" + ports + "]");
}

constexpr std::string_view k_abort_program =
    ":- dynamic(log/1).\n"
    "t.\n"
    "w :- setup_call_cleanup(true, with_output_to(atom(_), setup_call_cleanup(true, t, assertz(log(inner)))),\n"
    "    assertz(log(outer))).\n";

// with_output_to/2 の入れ子の実行の中の a。内側の実行が床（Barrier）より上の後始末を先に動かし、外の実行が残りを
// 動かす。どちらも 1 回だけで、デバッグの状態と abort_now は残る。
void check_abort_nested_cleanup() {
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(machine->load_program(k_abort_program).ok, "abort_nested_cleanup: load the program");
  impl.message_lang = mosa::MessageLanguage::En;
  Scripted script{" Call: (5) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, w.");
  mosa_test::check(stop.kind == mosa::StopKind::Abort,
                   "abort_nested_cleanup: Abort, got [" + stop_text(*machine, stop) + "], ports [" + script.text + "]");
  mosa_test::check(impl.abort_now, "abort_nested_cleanup: abort_now is kept");
  mosa_test::check(impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep, "abort_nested_cleanup: state kept");
  mosa_test::check(occurrences(script.text, ") assertz(") == 0,
                   "abort_nested_cleanup: no port in the cleanups, [" + script.text + "]");
  const mosa::Stop logged = machine->query("nodebug, findall(X, log(X), L).");
  const std::string text = stop_text(*machine, logged);
  mosa_test::check(text.find("L = [inner,outer]") != std::string::npos,
                   "abort_nested_cleanup: inner then outer, once each, got [" + text + "]");
}

constexpr std::string_view k_abort_rules_program =
    ":- dynamic(log/1).\n"
    "t.\n"
    "e :- setup_call_cleanup(true, setup_call_cleanup(true, setup_call_cleanup(true, t, fail), throw(oops)),\n"
    "    assertz(log(after))).\n"
    "d :- setup_call_cleanup(true, t, (nodebug, assertz(log(nodebug)))).\n";

// abort の後始末の失敗と誤りは捨て、残りの後始末も動かして abort を続ける（既定 A、open-items）。
void check_abort_cleanup_errors() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_abort_rules_program).ok, "abort_cleanup_errors: load the program");
  mosa::engine_impl(*machine).message_lang = mosa::MessageLanguage::En;
  Scripted script{" Call: (5) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, e.");
  mosa_test::check(stop.kind == mosa::StopKind::Abort,
                   "abort_cleanup_errors: Abort, got [" + stop_text(*machine, stop) + "], ports [" + script.text + "]");
  const std::string text = stop_text(*machine, machine->query("nodebug, findall(X, log(X), L)."));
  mosa_test::check(text.find("L = [after]") != std::string::npos, "abort_cleanup_errors: the rest ran, got [" + text + "]");
}

// 後始末がデバッガを切っても、abort の後のデバッガは打ったときのまま（マニュアルの「モードはそのまま」、既定 A）。
void check_abort_cleanup_keeps_mode() {
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(machine->load_program(k_abort_rules_program).ok, "abort_keeps_mode: load the program");
  impl.message_lang = mosa::MessageLanguage::En;
  Scripted script{" Call: (3) t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query("trace, d.");
  mosa_test::check(stop.kind == mosa::StopKind::Abort, "abort_keeps_mode: Abort, ports [" + script.text + "]");
  mosa_test::check(impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep,
                   "abort_keeps_mode: still creep after a cleanup that ran nodebug");
  const std::string text = stop_text(*machine, machine->query("nodebug, findall(X, log(X), L)."));
  mosa_test::check(text.find("L = [nodebug]") != std::string::npos, "abort_keeps_mode: the cleanup ran, got [" + text + "]");
}

constexpr std::string_view k_abort_bindings_program =
    ":- dynamic(log/1).\n"
    "t.\n"
    "b(X) :- setup_call_cleanup(S = s, (X = g, t), assertz(log(S-X))).\n"
    "n(X) :- setup_call_cleanup(true, setup_call_cleanup(true, (X = g, t), X = inner), assertz(log(X))).\n"
    "c :- catch(setup_call_cleanup(true, t, throw(oops)), _, assertz(log(caught))), assertz(log(went_on)).\n";

// goal を target のポートの a で止め、log を question で読んだ答えの字面を返す。
std::string abort_log(std::string_view label, std::string_view goal, std::string_view target, std::string_view question) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_abort_bindings_program).ok, std::string{label} + ": load the program");
  mosa::engine_impl(*machine).message_lang = mosa::MessageLanguage::En;
  Scripted script{std::string{target}, "a", {}, {}, false};
  give_script(*machine, script);
  const mosa::Stop stop = machine->query(std::string{goal} + ".");
  mosa_test::check(stop.kind == mosa::StopKind::Abort && script.used,
                   std::string{label} + ": Abort, got [" + stop_text(*machine, stop) + "], ports [" + script.text + "]");
  return stop_text(*machine, machine->query(std::string{question} + "."));
}

// abort の後始末は、例外が通るときと同じ束縛を見る（2026-11-18 の持ち主の決定 B）。Setup の束縛 S = s は見え、Goal の
// 束縛 X = g は見えない。入れ子なら内側の後始末の束縛 X = inner も、外側の後始末の前に戻る。前は abort の時点の束縛の
// まま動き、log(s-g) と log(g) が残った。
void check_abort_cleanup_bindings() {
  const std::string single = abort_log("abort_bindings", "trace, b(X)", " Call: (3) t ? ",
                                       "nodebug, findall(S-V, (log(S-X), (var(X) -> V = unbound ; V = X)), L)");
  mosa_test::check(single.find("L = [s-unbound]") != std::string::npos,
                   "abort_bindings: Setup's binding stays, Goal's is undone, got [" + single + "]");
  const std::string nested = abort_log("abort_bindings_nested", "trace, n(X)", " Call: (4) t ? ",
                                       "nodebug, findall(V, (log(X), (var(X) -> V = unbound ; V = X)), L)");
  mosa_test::check(nested.find("L = [unbound]") != std::string::npos,
                   "abort_bindings_nested: the inner cleanup's binding is undone too, got [" + nested + "]");
}

// abort の後始末の誤りは、下に残る catch/3 にも捕まらない。後始末を Entry の上で動かすので、受けずに投げると catch/3 が
// 捕まえ、止めたはずの c が先へ進んでしまう。
void check_abort_cleanup_uncaught() {
  const std::string text =
      abort_log("abort_uncaught", "trace, c", " Call: (3) t ? ", "nodebug, findall(X, log(X), L)");
  mosa_test::check(text.find("L = []") != std::string::npos,
                   "abort_uncaught: no catch/3 saw the cleanup's error, got [" + text + "]");
}

// 指令の中の a。指令が脇に置いたスタックの後始末を動かしてから読み込みを戻す。大域変数は読み込みの巻き戻しに
// 戻されないので、後始末が動いたことが見える。
void check_abort_directive_cleanup() {
  auto machine = make_machine();
  Scripted script{" Call: (1) dbg_t ? ", "a", {}, {}, false};
  give_script(*machine, script);
  mosa_test::check(machine->query("nb_setval(dbg_k, none).").kind == mosa::StopKind::Success,
                   "abort_directive_cleanup: nb_setval");
  const mosa::LoadStatus status =
      machine->load_program("dbg_t.\n:- setup_call_cleanup(true, (trace, dbg_t), nb_setval(dbg_k, done)).\n");
  mosa_test::check(!status.ok && script.used, "abort_directive_cleanup: the load is aborted, [" + script.text + "]");
  const mosa::Stop got = machine->query("nodebug, nb_getval(dbg_k, V).");
  const std::string text = stop_text(*machine, got);
  mosa_test::check(text.find("V = done") != std::string::npos, "abort_directive_cleanup: ran, got [" + text + "]");
}

// initialization/1 のゴールの abort。例外と同じく読み込みは失敗し、後の初期化は動かない。診断は書かない。節は
// 例外のときと同じく残る（持ち主の決定 A、2026-11-18）。指令で読んだファイルの分は abort_nested_* の記録で確かめる。
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

// 失敗した読み込みは leash/1 の変更も残さない（2026-11-18 の持ち主の決定 B。入り切りと進み方と同じ組で戻す）。
// 成功した読み込みの変更は実行の旗と同じく残る。
void check_leash_rollback() {
  auto machine = make_machine();
  const auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(machine->query("leash([none, call]).").kind == mosa::StopKind::Success,
                   "leash_rollback: set call only");
  const mosa::LoadStatus failed = machine->load_program(":- leash(all).\n:- fail.\n");
  mosa_test::check(!failed.ok, "leash_rollback: the load fails");
  mosa_test::check(impl.debug.leashed == 0x01, "leash_rollback: back to call only");
  const mosa::LoadStatus kept = machine->load_program(":- leash(-call).\n");
  mosa_test::check(kept.ok && impl.debug.leashed == 0x00, "leash_rollback: a good load keeps the change");
}

// 失敗した :- Goal の指令の中で打った n は、読み込みの巻き戻しで取り消される（CHANGELOG の既知の制限、持ち主の決定 B）。
// 対照に、同じ n を成功する指令の中で打てば残ることを見る。
bool nodebug_in_directive(std::string_view label, std::string_view text, bool& loaded) {
  auto machine = make_machine();
  const auto& impl = mosa::engine_impl(*machine);
  Scripted script{" Call: (1) dbg_t ? ", "n", {}, {}, false};
  give_script(*machine, script);
  mosa_test::check(machine->query("trace.").kind == mosa::StopKind::Success, std::string{label} + ": trace");
  loaded = machine->load_program(text).ok;
  mosa_test::check(script.used, std::string{label} + ": n was typed, [" + script.text + "]");
  return impl.debug_on && impl.debug.mode == mosa::DebugMode::Creep;
}

void check_directive_nodebug_rollback() {
  bool loaded = true;
  const bool undone = nodebug_in_directive("nodebug_rollback", "dbg_t.\n:- dbg_t, fail.\n", loaded);
  mosa_test::check(!loaded && undone, "nodebug_rollback: a failed directive undoes n");
  const bool kept_on = nodebug_in_directive("nodebug_kept", "dbg_t.\n:- dbg_t.\n", loaded);
  mosa_test::check(loaded && !kept_on, "nodebug_kept: n stays after a good load");
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

// コントローラの裁定 17。読み込む前に置いたスパイポイントは (user, append, 3) で覚えられ、use_module の後の呼び出しは
// (lists, append, 3) に引き当たる。置いた鍵を今引き直して合わせるので、leap はその呼び出しで止まる。nospy/1 も
// 同じ引き直しで外す。
void check_spy_before_load() {
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  impl.message_lang = mosa::MessageLanguage::En;
  Console console;
  give_console(*machine, console);
  mosa_test::check(machine->query("spy(append/3).").kind == mosa::StopKind::Success, "spy_before_load: spy");
  mosa_test::check(machine->query("use_module(library(lists)).").kind == mosa::StopKind::Success,
                   "spy_before_load: use_module");
  const mosa::Stop stop = machine->query("append([a], [b], L).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "spy_before_load: append succeeds");
  mosa_test::check(console.text.find(" Call: (1) append([a],[b],_G1) ? ") != std::string::npos,
                   "spy_before_load: leap stops at append, [" + console.text + "]");
  mosa_test::check(machine->query("nospy(append/3).").kind == mosa::StopKind::Success, "spy_before_load: nospy");
  mosa_test::check(impl.debug.spies.empty(), "spy_before_load: nospy removes the early spy point");
}

// spy を読み込みの前後で二度置いても、同じ述語の鍵は一つだけ。
void check_spy_twice_one_key() {
  auto machine = mosa::Machine::create();
  auto& impl = mosa::engine_impl(*machine);
  impl.message_lang = mosa::MessageLanguage::En;
  Console console;
  give_console(*machine, console);
  mosa_test::check(machine->query("spy(append/3), use_module(library(lists)), spy(append/3).").kind ==
                       mosa::StopKind::Success,
                   "spy_twice: query");
  mosa_test::check(impl.debug.spies.size() == 1, "spy_twice: one key, got " + std::to_string(impl.debug.spies.size()));
}

struct Traced {
  std::string ports;
  std::string answers;
};

// creep で答えを尽きるまで聞き、ポートの行と答えを返す。
Traced trace_goal(const std::string& goal) {
  auto machine = make_machine();
  mosa_test::check(machine->load_program("qa(L) :- lists:append([a], [b], L).\n").ok, "qualified: load qa/1");
  Console console;
  give_console(*machine, console);
  const std::string answers = joined(all_answers(*machine, goal));
  return Traced{console.text, answers};
}

// 利用者の文脈から Lib:G と書いたライブラリの呼び出しも、修飾しない append/3 と同じく 1 つの箱で、中の再帰は見せない
// （ADR 0028 の改訂、敵対的レビュー）。トップレベル、節の本体、call/1、call/N、leap のスパイポイントで同じになる。
void check_library_qualified() {
  const Traced top = trace_goal("trace, lists:append([a], [b], L)");
  mosa_test::check(top.ports.find(" Call: (1) append([a],[b],_G1) ? ") != std::string::npos &&
                       top.ports.find(" Exit: (1) append([a],[b],[a,b]) ? ") != std::string::npos &&
                       occurrences(top.ports, " Call: ") == 1,
                   "qualified: one box at the top, [" + top.ports + "]");
  const Traced body = trace_goal("trace, qa(L)");
  mosa_test::check(body.ports.find(" Call: (2) append([a],[b],_G1) ? ") != std::string::npos &&
                       occurrences(body.ports, " Call: ") == 2,
                   "qualified: one box in a clause body, [" + body.ports + "]");
  const Traced called = trace_goal("trace, call(lists:append([a], [b], L))");
  mosa_test::check(called.ports.find(" Call: (1) append([a],[b],_G1) ? ") != std::string::npos &&
                       occurrences(called.ports, " Call: ") == 1,
                   "qualified: one box through call/1, [" + called.ports + "]");
  const Traced extra = trace_goal("trace, call(lists:append([a]), [b], L)");
  mosa_test::check(extra.ports.find(" Call: (1) append([a],[b],_G1) ? ") != std::string::npos &&
                       occurrences(extra.ports, " Call: ") == 1,
                   "qualified: one box through call/3, [" + extra.ports + "]");
  const Traced leap = trace_goal("spy(lists:append/3), lists:append([a], [b], L)");
  mosa_test::check(leap.ports.find(" Call: (1) append([a],[b],_G1) ? ") != std::string::npos,
                   "qualified: leap stops at the spy point, [" + leap.ports + "]");
}

// Lib:G の箱のやり直し。答えごとに Redo と Exit が出て、最後に Fail が 1 回出る。system:G は見せない。
void check_library_qualified_redo() {
  const Traced redo = trace_goal("trace, lists:append(X, Y, [a])");
  mosa_test::check(occurrences(redo.ports, " Call: ") == 1 && occurrences(redo.ports, " Redo: (1) append(") == 2 &&
                       occurrences(redo.ports, " Exit: (1) append(") == 2 &&
                       occurrences(redo.ports, " Fail: (1) append(_G1,_G2,[a]) ? ") == 1,
                   "qualified: redo of the box, [" + redo.ports + "] " + redo.answers);
  const Traced hidden = trace_goal("trace, system:atom_length(abc, N)");
  mosa_test::check(hidden.ports.empty(), "qualified: system:G has no box, [" + hidden.ports + "]");
}

}  // namespace

int main() {
  check_trace_same_answers();
  check_trace_choice_depth();
  check_no_console();
  check_retry_cleanup();
  check_redo_retry_cleanup();
  check_redo_fail_cleanup();
  check_exception_fail_cleanup();
  check_exception_retry_cleanup();
  check_library_redefine();
  check_abort_stop();
  check_abort_cleanup();
  check_abort_nested_cleanup();
  check_abort_directive_cleanup();
  check_abort_cleanup_errors();
  check_abort_cleanup_keeps_mode();
  check_abort_cleanup_bindings();
  check_abort_cleanup_uncaught();
  check_abort_init();
  check_leash_rollback();
  check_directive_nodebug_rollback();
  check_skip_dropped();
  check_spy_before_load();
  check_spy_twice_one_key();
  check_library_qualified();
  check_library_qualified_redo();
  return mosa_test::failures() == 0 ? 0 : 1;
}
