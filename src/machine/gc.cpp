// ADR 0010。GC の方針（柔らかい容量と閾値）、安全点、ストレス、ルートの登録。印付けは gc_mark.cpp、
// 詰め直しは gc_compact.cpp。

#include "gc.hpp"

#include "internal.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

namespace mosa {
namespace {

// 上限の近くに残す余白。小さな上限でも GC の機会が上限より先に来るよう、上限の半分を超えない。
std::size_t headroom(std::size_t limit) { return std::min(std::max(limit / 4, std::size_t{65536}), limit / 2); }

std::size_t capacity_cap(std::size_t limit) { return limit - headroom(limit); }

// 7 割を超えたかを整数で比べる。浮動小数の丸めで境目が OS ごとに変わらないようにする。
bool crowded(std::size_t live, std::size_t capacity) { return live * 10 > capacity * 7; }

// 数字だけの正の整数。読めなければ 0（ストレスなし）。
std::size_t read_stress() {
  // std::getenv は MSVC で非推奨の警告になるので、platform の読み方を使う。
  const auto value = environment_path("MOSA_GC_STRESS");
  if (!value) return 0;
  const std::string text = utf8_from_path(*value);
  std::size_t every = 0;
  for (const char ch : text) {
    if (ch < '0' || ch > '9' || every > 100000000) return 0;
    every = every * 10 + static_cast<std::size_t>(ch - '0');
  }
  return every;
}

// 生きている量 L を見て C と閾値を決め直す。C の上でも 7 割を超えたままなら、閾値を L と上限の中ほどに置く。
// 毎回の安全点で GC が回り続けるのを防ぎ、いずれ確保が上限に当たって止まる。
// 1 歩で L が C の 1.4 倍を超えることがある（length/2 の長いリストなど）。1 回に 1 度だけ倍にすると、次の安全点で
// また混んでいて GC が続けて回るので、混まなくなるか上限に着くまで倍にし続ける。
std::size_t widened(std::size_t capacity, std::size_t occupied, std::size_t cap) {
  while (crowded(occupied, capacity) && capacity < cap) capacity = std::min(capacity * 2, cap);
  return capacity;
}

// 収集が外に持つ語はヒープの語と同じに数える（ADR 0010 の改訂 2）。L は前の GC の生きている語と今持つ語の和。
// gc_capacity は生きている語だけで広げた C で、持った語が無ければ改訂前と同じ値になる。持った語の分は呼ぶたびに
// その上へ広げ直すので、収集が終わって返した後に広がったままの C は残らない（レビュー A2）。
// 安全点の速い比較は heap.size() だけを見るので、trigger は閾値から持った語を引いた値にする。
// 生きている語は今のヒープより多くはない。前の GC の後にバックトラックで縮んだ分を数え続けると、持った語を
// 足したとき見積もりが上限を超え、回収できるゴミがあっても GC の前に止まる（Codex レビュー P2）。
// 縮めた見積もりは次の GC まで残す。その場の min だけだと、ゴミでヒープがまた伸びたときにゴミを生きている語に
// 数え直し、答えを足すたびに閾値が上限の側へ逃げて、GC の前に確保が上限に着く（Codex レビュー 2 回目の P2）。
// GC の直後は heap.size() が生きている語そのものなので、持った語が無ければ判断は改訂前と同じ。
void retune(Impl& machine) {
  const std::size_t limit = machine.heap.limit();
  const std::size_t cap = capacity_cap(limit);
  const std::size_t held = machine.heap.held();
  machine.gc_last_live = std::min(machine.gc_last_live, machine.heap.size());
  const std::size_t occupied = machine.gc_last_live + held;
  const std::size_t capacity = widened(machine.gc_capacity, occupied, cap);
  machine.gc_threshold = capacity;
  if (capacity >= cap && crowded(occupied, capacity)) {
    machine.gc_threshold = occupied >= limit ? occupied : std::max(capacity, occupied + (limit - occupied) / 2);
  }
  machine.gc_tuned_held = held;
  if (machine.gc_stress != 0) machine.gc_trigger = 0;
  else machine.gc_trigger = held < machine.gc_threshold ? machine.gc_threshold - held : 0;
}

void update_policy(Impl& machine, std::size_t live) {
  machine.gc_capacity = widened(machine.gc_capacity, live, capacity_cap(machine.heap.limit()));
  machine.gc_last_live = live;
  retune(machine);
}

}  // namespace

void GcVisitor::env(EnvCells* env) {
  if (env == nullptr || env->gc_pass == pass_) return;
  env->gc_pass = pass_;
  for (Cell& slot : env->cells) cell(slot);
}

void GcVisitor::collect(Collector* collector) {
  if (collector == nullptr || collector->gc_pass == pass_) return;
  collector->gc_pass = pass_;
  collector->visit(*this);
}

void GcVisitor::entry(Entry& entry) {
  env(entry.env.get());
  collect(entry.collect.get());
  cell(entry.pattern);
  frame(entry.redo_goal);
  frame(entry.cont);
  heap_mark(entry.heap_mark);
  trail_mark(entry.trail_mark);
  extras_mark(entry.extras_mark);
}

void visit_roots(Impl& machine, GcVisitor& visitor) {
  visitor.frame(machine.ip);
  visitor.cell(machine.ball);
  for (Binding& binding : machine.bindings) visitor.cell(binding.variable);
  for (Entry& entry : machine.stack) visitor.entry(entry);
  for (PendingCleanup& pending : machine.pending_cleanups) visitor.cell(pending.goal);
  for (const RootVisit& visit : machine.gc_roots) visit(visitor);
}

RootScope::RootScope(Impl& machine, RootVisit visit) : machine_(machine) {
  machine_.gc_roots.push_back(std::move(visit));
}

// 入れ子は内側から抜けるので、最後に積んだものが自分である。
RootScope::~RootScope() { machine_.gc_roots.pop_back(); }

void init_gc(Impl& machine) {
  const std::size_t limit = machine.heap.limit();
  machine.gc_capacity = std::min(kInitialHeapCommit, capacity_cap(limit));
  machine.gc_threshold = machine.gc_capacity;
  machine.gc_stress = read_stress();
  machine.gc_trigger = machine.gc_stress == 0 ? machine.gc_threshold : 0;
}

namespace {

// 行の型。数の位置に印の文字 U+0001〜U+0005 を渡してカタログから作り、言語ごとに 1 度だけ作り直す。カタログを
// 引くたびに新しいマシンを作るので、1 行ごとに引くと 1.5 ms ほどかかる。~w は原子をそのまま書くので、印を数に
// 置き換えた行はカタログで直に作った行と同じバイト列になり、書式を C++ に写さずに済む。
const std::string& trace_form(Impl& machine) {
  if (!machine.gc_trace_form || machine.gc_trace_form_lang != machine.message_lang) {
    machine.gc_trace_form = message_text(machine, "gc_trace", {"\x01", "\x02", "\x03", "\x04", "\x05"});
    machine.gc_trace_form_lang = machine.message_lang;
  }
  return *machine.gc_trace_form;
}

}  // namespace

std::string gc_trace_text(Impl& machine, std::span<const std::string> numbers) {
  std::string line;
  for (const char ch : trace_form(machine)) {
    const auto slot = static_cast<std::size_t>(static_cast<unsigned char>(ch));
    if (slot >= 1 && slot <= numbers.size()) {
      line += numbers[slot - 1];
    } else {
      line += ch;
    }
  }
  return line;
}

// 旗 trace_gc の 1 行。文面は鍵 gc_trace のカタログから引く。ミリ秒は小数 2 桁まで見せる。
void trace_line(Impl& machine, std::size_t before, std::size_t after, std::chrono::steady_clock::duration took) {
  const auto kib = [](std::size_t words) { return std::to_string(words * kHeapWordBytes / 1024); };
  const auto hundredths = std::chrono::duration_cast<std::chrono::microseconds>(took).count() / 10;
  const std::string ms = std::to_string(hundredths / 100) + "." + (hundredths % 100 < 10 ? "0" : "") +
                         std::to_string(hundredths % 100);
  const std::array<std::string, 5> numbers{std::to_string(machine.gc_stats.count), kib(before), kib(after),
                                           kib(machine.gc_capacity), ms};
  std::cerr << gc_trace_text(machine, numbers) << '\n';
}

void collect_garbage(Impl& machine) {
  const auto started = std::chrono::steady_clock::now();
  const std::size_t before = machine.heap.size();
  if (!machine.exception) machine.ball = Cell{};
  GcMarks marks;
  mark_live(machine, marks);
  compact_live(machine, marks);
  const std::size_t after = machine.heap.size();
  machine.gc_stats.count += 1;
  machine.gc_stats.freed_words += before - after;
  machine.gc_stats.time += std::chrono::steady_clock::now() - started;
  update_policy(machine, after);
  if (machine.trace_gc) trace_line(machine, before, after, std::chrono::steady_clock::now() - started);
}

namespace {

// 空きを生まない GC の基準（ADR 0010 の 2026-10-30 改訂、所有者の決定 B）。上限の 1/32 は、生きているデータが
// 上限まで 1/16 ほどに迫ったときの 1 回の空き（閾値は L と上限の中ほど）にあたる。3 回は、1 回だけ空きが少ない
// 波（大きな項を作って捨てる途中）で止めないための猶予である。
constexpr std::size_t kUnproductiveDivisor = 32;
constexpr std::size_t kUnproductiveRun = 3;

// 閾値で始めた GC の後に呼ぶ。C が天井にあり、前の判定から空いた語が上限の 1/32 に満たない GC が続けば、上限での
// 確保と同じ ResourceLimit を投げる。空きを前の判定から数えるのは、間のストレスや garbage_collect/0 の GC が先に
// 回収した分もこの閾値までの空きだからである。この GC の分だけで見ると、ストレスの有無で止まるかが分かれる。
// 安全点はステップの間なので、途中まで進んだ C++ のコードは無く、受ける場所も同じで済む。
// 投げる前に数を 0 に戻し、止めた問い合わせの数を受けた側の次の GC へ持ち越さない。
void judge_collection(Impl& machine, bool at_cap) {
  const std::uint64_t freed = machine.gc_stats.freed_words - machine.gc_judged_freed;
  machine.gc_judged_freed = machine.gc_stats.freed_words;
  if (!at_cap || freed >= machine.heap.limit() / kUnproductiveDivisor) {
    machine.gc_unproductive = 0;
    return;
  }
  if (++machine.gc_unproductive < kUnproductiveRun) return;
  machine.gc_unproductive = 0;
  throw ResourceLimit("heap_limit");
}

}  // namespace

// 収集が返した語は Collecting の破棄で Heap へ戻るだけなので、閾値と trigger は前の値のまま来る。ここで直す。
void gc_safe_point(Impl& machine) {
  if (machine.heap.held() != machine.gc_tuned_held) retune(machine);
  const std::size_t occupied = machine.heap.size() + machine.heap.held();
  if (machine.gc_stress == 0 && occupied < machine.gc_threshold) return;
  const bool by_threshold = occupied >= machine.gc_threshold;
  bool due = by_threshold;
  if (machine.gc_stress != 0 && ++machine.gc_ticks >= machine.gc_stress) {
    machine.gc_ticks = 0;
    due = true;
  }
  if (!due) return;
  // ストレスだけで来た GC は上限の近さと関係なく回るので判定しない（空いた分は次の判定に入る）。
  if (!by_threshold) {
    collect_garbage(machine);
    return;
  }
  // 閾値が天井より下なのは、持った語を足しても C が天井に着いていないときだけ。持った語が無ければ改訂前と同じ判定。
  const bool at_cap = machine.gc_threshold >= capacity_cap(machine.heap.limit());
  collect_garbage(machine);
  judge_collection(machine, at_cap);
}

// 答えを足したあと。ヒープの語と持った語の和が上限を超えれば、ヒープの確保と同じ停止にする。
void hold_answer_words(Impl& machine, std::size_t words) {
  machine.heap.hold(words);
  retune(machine);
  const std::size_t limit = machine.heap.limit();
  if (machine.heap.held() > limit || machine.heap.size() > limit - machine.heap.held()) {
    throw ResourceLimit("heap_limit");
  }
}

// 前の問い合わせや読み込みの GC の空きを、次の判定に持ち越さない。
void reset_gc_judgement(Impl& machine) {
  machine.gc_unproductive = 0;
  machine.gc_judged_freed = machine.gc_stats.freed_words;
}

}  // namespace mosa
