// 非 ISO（処理系の拡張）。garbage_collect/0 と statistics/2。SICStus と SWI の形に合わせる。
// garbage_collect/0
//   引数なし。ヒープを今すぐ詰める。投げる誤りは無い。
// statistics(+atom, -term)
//   instantiation_error（鍵が変数）、type_error(atom, K)、domain_error(statistics_key, K)
//   鍵: garbage_collection -> [回数, 回収したバイト, ミリ秒]、heap -> [使用バイト, 柔らかい容量のバイト, 上限のバイト]、
//   runtime / walltime -> [合計ミリ秒, 前回の同じ鍵からの差]（前回はマシンごと）、cputime -> CPU 秒（浮動小数）。
// テスト: tests/pl/gc.pl、tests/cpp/gc_test.cpp（check_trace_line）
#include "../machine/internal.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <iterator>
#include <string_view>

namespace mosa {
namespace {

constexpr auto kWordBytes = static_cast<std::int64_t>(kHeapWordBytes);

Cell make_list(Impl& machine, std::initializer_list<std::int64_t> values) {
  Cell list = machine.heap.atom(machine.atoms.intern("[]"));
  for (auto it = std::rbegin(values); it != std::rend(values); ++it) {
    list = machine.heap.list(machine.heap.integer(*it), list);
  }
  return list;
}

std::int64_t cpu_ms() {
  return static_cast<std::int64_t>(std::clock()) * 1000 / static_cast<std::int64_t>(CLOCKS_PER_SEC);
}

std::int64_t wall_ms(const Impl& machine) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - machine.started_at)
      .count();
}

// 合計と、前回の読みからの差。前回の値は読むたびに更新する。
Cell total_and_delta(Impl& machine, std::int64_t total, std::int64_t& last) {
  const std::int64_t delta = total - last;
  last = total;
  return make_list(machine, {total, delta});
}

// 鍵が知らない名前なら false。値は鍵ごとに作る。
bool statistic_value(Impl& machine, std::string_view key, Cell& value) {
  if (key == "garbage_collection") {
    const auto& stats = machine.gc_stats;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(stats.time).count();
    value = make_list(machine, {static_cast<std::int64_t>(stats.count),
                                static_cast<std::int64_t>(stats.freed_words) * kWordBytes, ms});
  } else if (key == "heap") {
    value = make_list(machine, {static_cast<std::int64_t>(machine.heap.size()) * kWordBytes,
                                static_cast<std::int64_t>(machine.gc_capacity) * kWordBytes,
                                static_cast<std::int64_t>(machine.heap.limit()) * kWordBytes});
  } else if (key == "runtime") {
    value = total_and_delta(machine, cpu_ms(), machine.last_runtime_ms);
  } else if (key == "walltime") {
    value = total_and_delta(machine, wall_ms(machine), machine.last_walltime_ms);
  } else if (key == "cputime") {
    value = machine.heap.floating(static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC));
  } else {
    return false;
  }
  return true;
}

void raise(Impl& machine, const Goal& goal, Cell formal) { raise_formal(machine, formal, goal.name, goal.arity); }

void run_statistics_2(Impl& machine, const Goal& goal) {
  const Cell key = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(key)) return raise(machine, goal, formal_instantiation(machine));
  if (key.tag() != Tag::Atom) return raise(machine, goal, formal_type(machine, "atom", key));
  Cell value;
  if (!statistic_value(machine, machine.atoms.name(static_cast<std::uint32_t>(key.raw() >> 3)), value)) {
    return raise(machine, goal, formal_domain(machine, "statistics_key", key));
  }
  // 束縛は最後。ここまでに失敗する道は無い。
  if (do_unify(machine, goal.arg(machine, 1), value)) machine.ip = goal.cont;
  else machine.failed = true;
}

}  // namespace

bool run_statistics(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "garbage_collect" && goal.arity == 0) {
    // 組込は step の中で動く。この後 GC が動かすセルを握るのは goal.cont だけなので、先に ip へ置く
    // （ip は GC のルート）。以後このゴールの枠も goal も読まない。
    machine.ip = goal.cont;
    collect_garbage(machine);
    return true;
  }
  if (name == "statistics" && goal.arity == 2) {
    run_statistics_2(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
