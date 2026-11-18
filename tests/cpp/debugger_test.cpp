// フェーズ 14（ADR 0028）。追跡しても答えが変わらないこと（カット、findall/3、catch/3、setup_call_cleanup/3、call/N）、
// 決定的な問い合わせの後に番兵が残らないこと（has_more は箱の選択点を数えないので、; の問いでは見つからない）、
// DebugConsole の無い機械（mosatest、mosac の実行ファイル、埋め込み）は trace の後も箱を作らないこと。
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

}  // namespace

int main() {
  check_trace_same_answers();
  check_trace_choice_depth();
  check_no_console();
  return mosa_test::failures() == 0 ? 0 : 1;
}
