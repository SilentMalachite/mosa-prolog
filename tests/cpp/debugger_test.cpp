// フェーズ 14（ADR 0028）。追跡しても答えが変わらないこと（カット、findall/3、catch/3、setup_call_cleanup/3、call/N）、
// 決定的な問い合わせの後に番兵が残らないこと（has_more は箱の選択点を数えないので、; の問いでは見つからない）、
// DebugConsole の無い機械（mosatest、mosac の実行ファイル、埋め込み）は trace の後も箱を作らないこと、
// setup_call_cleanup/3 の選択点を中に残した箱を r と f で切っても後始末が外された回数だけ動くこと（タスク 6）、
// 定義が見える前に置いたスパイポイントが、後で読み込んだ述語の呼び出しで止まること（タスク 8）、利用者が Lib:G と
// 書いたライブラリの呼び出しが 1 つの箱になること（ADR 0028 の改訂）、デバッガの述語の節は permission_error で読み込みを
// 止めること（SPEC 第5節）。a は debugger_abort_test.cpp が見る。
// ポートの行そのものはトランスクリプト（tests/cpp/data/debugger）で確かめる。
#include "debugger_support.hpp"

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

using namespace debugger_support;

// 書いた行を覚える。読む行はいつも空で、どのポートでも creep になる。
struct Console {
  std::string text;
  std::size_t writes = 0;
};

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

// デバッガの 9 つの述語は system の述語なので、利用者の節は permission_error で読み込みを止める（SPEC 第5節）。
// tests/pl/debugger.pl の dbg_redefine は consult の失敗しか見ないので、誤りの形はここで見る（PR #4 の 2 度目の
// 敵対的レビュー）。
void check_redefine_permission() {
  struct Case {
    std::string_view clause;
    std::string_view indicator;
  };
  constexpr std::array<Case, 9> cases{{{"trace.", "trace/0"},
                                       {"notrace.", "notrace/0"},
                                       {"debug.", "debug/0"},
                                       {"nodebug.", "nodebug/0"},
                                       {"spy(_).", "spy/1"},
                                       {"nospy(_).", "nospy/1"},
                                       {"nospyall.", "nospyall/0"},
                                       {"leash(_).", "leash/1"},
                                       {"debugging.", "debugging/0"}}};
  for (const Case& item : cases) {
    auto machine = mosa::Machine::create();
    const mosa::LoadStatus status = machine->load_program(std::string{item.clause} + "\n");
    const std::string want = "permission_error(modify,static_procedure," + std::string{item.indicator} + ")";
    mosa_test::check(!status.ok && status.message.find(want) != std::string::npos,
                     "redefine_permission: " + std::string{item.clause} + " got [" + status.message + "]");
  }
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
  check_redefine_permission();
  check_leash_rollback();
  check_directive_nodebug_rollback();
  check_skip_dropped();
  check_spy_before_load();
  check_spy_twice_one_key();
  check_library_qualified();
  check_library_qualified_redo();
  return mosa_test::failures() == 0 ? 0 : 1;
}
