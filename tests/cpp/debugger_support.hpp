#pragma once

// debugger_test.cpp と debugger_abort_test.cpp が共に使う機械と口（フェーズ 14、ADR 0028）。読む行を台本で返す口、
// 答えの字面、setup_call_cleanup/3 の後始末を数える問い合わせを置く。2 つのテストは同じ形でポートを見るので、写しを
// 2 つ持たない。

#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace debugger_support {

constexpr std::string_view k_program =
    ":- use_module(library(lists)).\n"
    "len([], 0).\n"
    "len([_|T], N) :- len(T, M), N is M + 1.\n";

inline std::unique_ptr<mosa::Machine> make_machine() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, "load len/2 and library(lists)");
  // 行の字面を LANG に左右させない。
  mosa::engine_impl(*machine).message_lang = mosa::MessageLanguage::En;
  return machine;
}

inline std::string stop_text(const mosa::Machine& machine, const mosa::Stop& stop) {
  if (stop.kind == mosa::StopKind::Success) return mosa::format_answer(machine, stop) + (stop.more ? " ;" : " .");
  if (stop.kind == mosa::StopKind::Failure) return "false";
  if (stop.kind == mosa::StopKind::Abort) return "abort";
  // 投げられた項も書く。違う誤りが同じ字面にならないため（Muse のレビュー）。
  if (stop.kind == mosa::StopKind::Exception) {
    return "exception " + mosa::write_term(machine.heap(), mosa::OperatorView{machine.operators()}, stop.exception, {}, {});
  }
  return "halt";
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

inline void give_script(mosa::Machine& machine, Scripted& script) {
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

inline std::size_t occurrences(const std::string& text, std::string_view part) {
  std::size_t count = 0;
  for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++count;
  return count;
}

// 後始末の回数。この問い合わせは前の問い合わせを閉じるので、閉じたときの後始末も数に入る。
inline std::string cleanup_count(mosa::Machine& machine) {
  const mosa::Stop stop = machine.query("nodebug, findall(x, cleaned, L), length(L, N).");
  return stop_text(machine, stop);
}

// cleanup_count の答えの N が n か。字面の途中を探すと N = 20 も N = 2 に合うので、答えの終わりで比べる（Muse の
// レビュー）。
inline bool count_is(const std::string& count, int n) { return count.ends_with("N = " + std::to_string(n) + " ."); }

struct CleanupCase {
  std::string_view label;
  std::string_view goal;
  std::string_view target;
  std::string_view command;
};

// Review Focus 4。setup_call_cleanup/3 の選択点を中に残した箱を r か f で切っても、後始末は外された回数だけ動く。
inline std::string run_cleanup_case(const CleanupCase& item, std::string& answer, std::string& ports) {
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

}  // namespace debugger_support
