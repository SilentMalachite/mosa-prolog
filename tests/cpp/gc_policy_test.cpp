// gc_test.cpp の続き（800 行の上限で分けた）。上限の近くで空きを生まない GC が続けば止める判定（ADR 0010 の
// 2026-10-30 改訂）、ストレスの GC の有無で止まるかが分かれないこと、判定の基準を問い合わせごとに戻すこと、GC の
// 知らせの行と文面を見る。テストは無名の名前空間に置き、呼ばれないテストは -Werror の unused-function で落ちる。
// main は gc_test.cpp にあり、gc_policy_checks をそこでの順のまま呼ぶ。特に断らない限り上限は 2^16 語。

#include "gc_support.hpp"
#include "harness.hpp"

#include "machine/gc.hpp"
#include "machine/internal.hpp"
#include "mosa/machine.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>

namespace {

using gc_support::ask;
using gc_support::consulted_path;
using gc_support::k_loop;
using gc_support::k_small;

// 上限の近くで空きを生まない GC が続けば止める（ADR 0010 の 2026-10-30 改訂、所有者の決定 B）。
// junk/1 は 1 歩ごとに捨てる項を作り、grow/2 は生きたリストを 1 要素ずつ伸ばす。
constexpr std::string_view k_thrash =
    "junk(0) :- !. junk(K) :- _ = f(K, K, K, K), K1 is K - 1, junk(K1).\n"
    "grow(N, L) :- junk(5), N1 is N + 1, grow(N1, [N|L]).";

// 例外の文面も返す。ResourceLimit の文面は止め方の鍵（heap_limit）である。
std::string ask_stop_text(mosa::Machine& machine, std::string_view goal) {
  try {
    (void)machine.query(goal);
  } catch (const mosa::ResourceLimit& stop) {
    return stop.what();
  }
  return {};
}

// length/2 の 2 万 1 千要素（6 万 3 千語）で上限 2^16 語の 96% を 1 歩で生かし、捨てる項を作りながら伸ばす。
// 空きは 2536 語で上限の 1/16（4096 語）より狭いので、閾値は L と上限の中ほどにあり、閾値で始まる GC は
// 1 歩の確保を足しても 2048 語（上限の 1/32）を空けられない。length の直後の GC（空きは 0）から 3 回で止まる。
// 改訂の前は、空きが 1 歩の確保を下回って確保が上限に当たるまで、何百回も GC が続いた。
void check_thrash_stops() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_thrash).ok, "thrash: load");
  auto& impl = mosa::engine_impl(*machine);
  const auto before = impl.gc_stats.count;
  const std::string text = ask_stop_text(*machine, "length(L, 21000), grow(0, L).");
  mosa_test::check(text == "heap_limit", "thrash: stops with heap_limit (got '" + text + "')");
  const auto count = impl.gc_stats.count - before;
  // ストレスでは回数を比べず、どちらの止まり方かも問わない。ストレスの GC が閾値より先に捨てる項を回収するので、
  // 閾値に届くかは N による（N = 1 なら閾値に届かず、生きたリストが伸びて確保が上限に当たる）。どちらも同じ
  // ResourceLimit("heap_limit") で、止まったときのヒープの大きさでも見分けられない（確保は上限を超える前に断る）。
  if (impl.gc_stress == 0) mosa_test::check(count <= 3, "thrash: at most 3 GCs (got " + std::to_string(count) + ")");
}

// 生きているデータが上限の半分ほど（3 万語）なら、1 回の GC で上限の 1/4 ほどが空くので止めない。
void check_productive_near_limit() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "productive: load");
  auto& impl = mosa::engine_impl(*machine);
  bool limited = false;
  const auto stop = ask(*machine, "length(L, 10000), loop(1000000), L = [_|_].", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "productive: finishes without a stop");
  mosa_test::check(impl.gc_stats.count >= 1, "productive: GC ran");
}

// garbage_collect/0 の GC は数えない。6 万 3 千語を生かしたまま何度呼んでも、空きを生まないことで止めない。
void check_explicit_collect_not_counted() {
  auto machine = mosa::Machine::create(k_small);
  bool limited = false;
  const auto stop = ask(*machine,
                        "length(L, 21000), garbage_collect, garbage_collect, garbage_collect, garbage_collect, "
                        "garbage_collect, L = [_|_].",
                        limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "explicit collect: not counted");
}

// 生きているデータが上限の 9 割ほど（1 万 9500 要素、5 万 8500 語、89%）でも、空きは上限の 1/16 より広く、
// 閾値で始まる GC は上限の 1/32 より多く空けるので止めない。
void check_productive_at_ninety() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "ninety: load");
  bool limited = false;
  const auto stop = ask(*machine, "length(L, 19500), loop(1000000), L = [_|_].", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "ninety: finishes without a stop");
}

// 3 回に分けて上限の 88% まで生かし、その間に捨てる項を作る（再レビューの diverge.pl を半分の上限に縮めたもの）。
// ストレスの GC が捨てる項を先に回収すると、続く閾値の GC は自分ではほとんど空けない。判定を前の判定からの空き
// （ストレスの GC の分も含む）で数えるので、ストレスの有無で止まるかが分かれない。
constexpr std::string_view k_diverge =
    "length(A, 11000), junk(3000), length(B, 5500), junk(3000), length(C, 2700), junk(3000), "
    "A = [_|_], B = [_|_], C = [_|_].";

void check_stress_same_outcome() {
  for (const std::size_t every : {std::size_t{0}, std::size_t{10}, std::size_t{1000}}) {
    const std::string label = "diverge stress " + std::to_string(every);
    auto machine = mosa::Machine::create(k_small);
    mosa_test::check(machine->load_program(k_thrash).ok, label + ": load");
    auto& impl = mosa::engine_impl(*machine);
    // 環境の MOSA_GC_STRESS によらず、この機械だけ N 回の安全点ごとに GC する（init_gc の後で上書きする）。
    if (every != 0) {
      impl.gc_stress = every;
      impl.gc_ticks = 0;
      impl.gc_trigger = 0;
    }
    bool limited = false;
    const auto stop = ask(*machine, k_diverge, limited);
    mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, label + ": finishes without a stop");
  }
}

// 数と空きの基準は、トップレベルの問い合わせ・consult・load_program のたびに戻す。前の分を次へ持ち越さない。
void check_unproductive_reset() {
  auto machine = mosa::Machine::create(k_small);
  auto& impl = mosa::engine_impl(*machine);
  const std::string file = consulted_path("reset.pl", "r(1).\n");
  const auto reset = [&impl] { return impl.gc_unproductive == 0 && impl.gc_judged_freed == impl.gc_stats.freed_words; };
  impl.gc_unproductive = 2;
  impl.gc_judged_freed = impl.gc_stats.freed_words + 1;
  const auto stop = machine->query("true.");
  mosa_test::check(stop.kind == mosa::StopKind::Success && reset(), "unproductive count resets at a query");
  impl.gc_unproductive = 2;
  impl.gc_judged_freed = impl.gc_stats.freed_words + 1;
  mosa_test::check(machine->consult(file) && reset(), "unproductive count resets at consult");
  impl.gc_unproductive = 2;
  impl.gc_judged_freed = impl.gc_stats.freed_words + 1;
  mosa_test::check(machine->load_program("s(1).").ok && reset(), "unproductive count resets at load_program");
}

// 標準エラーの流れを一時的に差し替え、例外で抜けても元へ戻す。
class CerrRedirect {
 public:
  explicit CerrRedirect(std::ostringstream& sink) : saved_(std::cerr.rdbuf(sink.rdbuf())) {}
  ~CerrRedirect() { std::cerr.rdbuf(saved_); }
  CerrRedirect(const CerrRedirect&) = delete;
  CerrRedirect& operator=(const CerrRedirect&) = delete;

 private:
  std::streambuf* saved_;
};

// 標準エラーを奪って goal を問い合わせ、出た文字列を返す。成功しなければ ok を偽にする。
std::string ask_capturing_stderr(mosa::Machine& machine, std::string_view goal, bool& ok) {
  bool limited = false;
  std::ostringstream captured;
  mosa::Stop stop;
  {
    const CerrRedirect redirect(captured);
    stop = ask(machine, goal, limited);
  }
  ok = !limited && stop.kind == mosa::StopKind::Success;
  return captured.str();
}

// 旗 trace_gc が true なら、GC のたびに標準エラーへ GC を含む 1 行が出る。偽に戻せば止まる。
void check_trace_line() {
  auto machine = mosa::Machine::create(k_small);
  const auto& impl = mosa::engine_impl(*machine);
  bool ok = false;
  const std::string quiet = ask_capturing_stderr(*machine, "garbage_collect.", ok);
  mosa_test::check(ok && quiet.empty(), "trace: silent by default");
  const std::string logged =
      ask_capturing_stderr(*machine, "set_prolog_flag(trace_gc, true), garbage_collect.", ok);
  mosa_test::check(ok, "trace: set true and collect succeeds");
  mosa_test::check(logged.find("GC") != std::string::npos, "trace: a GC line on stderr");
  // ストレスでは安全点ごとに GC が回るので、行の数は固定しない。
  if (impl.gc_stress == 0) {
    mosa_test::check(std::count(logged.begin(), logged.end(), '\n') == 1, "trace: exactly one line");
  }
  (void)ask_capturing_stderr(*machine, "current_prolog_flag(trace_gc, true).", ok);
  mosa_test::check(ok, "trace: flag reads back true");
  // 旗を戻す問い合わせの前の安全点ではまだ真なので、その出力は捨てる。次の問い合わせは旗が偽から始まるので、
  // ストレスでも行は出ない。
  (void)ask_capturing_stderr(*machine, "set_prolog_flag(trace_gc, false).", ok);
  mosa_test::check(ok, "trace: set false succeeds");
  const std::string stopped = ask_capturing_stderr(*machine, "garbage_collect.", ok);
  mosa_test::check(ok, "trace: collect after switching off succeeds");
  mosa_test::check(stopped.find("GC") == std::string::npos, "trace: no GC line after switching off");
}

// trace_gc の行はカタログで直に作った文面とバイトまで同じで、言語を変えれば従う。行ごとに新しいマシンで
// カタログを読み直すと 1 行 1.5 ms ほどかかるので、2000 行が 0.5 秒に収まることで読み直していないことを見る。
void check_trace_text() {
  const std::array<std::string, 5> numbers{"7", "1024", "512", "2048", "1.25"};
  auto machine = mosa::Machine::create(k_small);
  auto& impl = mosa::engine_impl(*machine);
  for (const auto language : {mosa::MessageLanguage::En, mosa::MessageLanguage::Ja, mosa::MessageLanguage::En}) {
    impl.message_lang = language;
    const std::string label = language == mosa::MessageLanguage::Ja ? "trace text ja" : "trace text en";
    const std::string expected =
        mosa::message_text(impl, "gc_trace", {numbers[0], numbers[1], numbers[2], numbers[3], numbers[4]});
    mosa_test::check(!expected.starts_with("gc_trace:") && expected.find("1024") != std::string::npos,
                     label + ": the catalog renders the line");
    mosa_test::check(mosa::gc_trace_text(impl, numbers) == expected, label + ": same bytes as the catalog");
    const auto started = std::chrono::steady_clock::now();
    for (int index = 0; index < 2000; ++index) (void)mosa::gc_trace_text(impl, numbers);
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    mosa_test::check(took.count() < 500, label + ": 2000 lines in " + std::to_string(took.count()) + " ms");
  }
}

}  // namespace

void gc_policy_checks() {
  check_thrash_stops();
  check_productive_near_limit();
  check_explicit_collect_not_counted();
  check_productive_at_ninety();
  check_stress_same_outcome();
  check_unproductive_reset();
  check_trace_line();
  check_trace_text();
}
