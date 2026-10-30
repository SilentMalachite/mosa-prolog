// ADR 0010。GC の方針（柔らかい容量と閾値）、安全点、ストレス、ルートの登録。印付けは gc_mark.cpp、
// 詰め直しは gc_compact.cpp。

#include "gc.hpp"

#include "internal.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
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
void update_policy(Impl& machine, std::size_t live) {
  const std::size_t limit = machine.heap.limit();
  const std::size_t cap = capacity_cap(limit);
  std::size_t capacity = machine.gc_capacity;
  if (crowded(live, capacity)) capacity = std::min(capacity * 2, cap);
  machine.gc_capacity = capacity;
  machine.gc_threshold = capacity;
  if (capacity >= cap && crowded(live, capacity)) {
    machine.gc_threshold = std::max(capacity, live + (limit - live) / 2);
  }
}

}  // namespace

void GcVisitor::entry(Entry& entry) {
  for (Cell& slot : entry.env) cell(slot);
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
  for (const RootVisit& visit : machine.gc_roots) visit(visitor);
}

RootScope::RootScope(Impl& machine, RootVisit visit) : machine_(machine) {
  machine_.gc_roots.push_back(std::move(visit));
}

// 入れ子は内側から抜けるので、最後に積んだものが自分である。
RootScope::~RootScope() { machine_.gc_roots.pop_back(); }

GcInhibit::GcInhibit(Impl& machine) : machine_(machine) { ++machine_.gc_inhibit; }

GcInhibit::~GcInhibit() { --machine_.gc_inhibit; }

void init_gc(Impl& machine) {
  const std::size_t limit = machine.heap.limit();
  machine.gc_capacity = std::min(kInitialHeapCommit, capacity_cap(limit));
  machine.gc_threshold = machine.gc_capacity;
  machine.gc_stress = read_stress();
}

// 旗 trace_gc の 1 行。文面は鍵 gc_trace のカタログから引く。ミリ秒は小数 2 桁まで見せる。
void trace_line(const Impl& machine, std::size_t before, std::size_t after,
                std::chrono::steady_clock::duration took) {
  const auto kib = [](std::size_t words) { return std::to_string(words * 8 / 1024); };
  const auto hundredths = std::chrono::duration_cast<std::chrono::microseconds>(took).count() / 10;
  const std::string ms = std::to_string(hundredths / 100) + "." + (hundredths % 100 < 10 ? "0" : "") +
                         std::to_string(hundredths % 100);
  const std::string line = message_text(machine, "gc_trace",
                                        {std::to_string(machine.gc_stats.count), kib(before), kib(after),
                                         kib(machine.gc_capacity), ms});
  std::cerr << line << '\n';
}

void collect_garbage(Impl& machine) {
  if (machine.gc_inhibit > 0) return;
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

void gc_safe_point(Impl& machine) {
  if (machine.gc_stress == 0 && machine.heap.size() < machine.gc_threshold) return;
  bool due = machine.heap.size() >= machine.gc_threshold;
  if (machine.gc_stress != 0 && ++machine.gc_ticks >= machine.gc_stress) {
    machine.gc_ticks = 0;
    due = true;
  }
  if (due) collect_garbage(machine);
}

}  // namespace mosa
