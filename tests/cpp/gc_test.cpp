// mark-and-compact の GC（ADR 0010、計画 Task 4）。ヒープ・extras・トレイルが頭打ちになり、生きている項が
// 値も形も変わらずに残ること。特に断らない限り上限は 2^16 語で、GC が無ければ loop は heap_limit で止まる。
#include "harness.hpp"

#include "machine/gc.hpp"
#include "machine/internal.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_small = std::size_t{1} << 16;

constexpr std::string_view k_loop = "loop(0) :- !. loop(N) :- N1 is N - 1, loop(N1).";
constexpr std::string_view k_loop_three = "loop(0) :- !. loop(N) :- N > 0, N1 is N - 1, loop(N1).";
constexpr std::string_view k_tak =
    "tak(X, Y, Z, A) :- X =< Y, !, Z = A.\n"
    "tak(X, Y, Z, A) :- X1 is X - 1, Y1 is Y - 1, Z1 is Z - 1,\n"
    "  tak(X1, Y, Z, A1), tak(Y1, Z, X, A2), tak(Z1, X, Y, A3), tak(A1, A2, A3, A).";
constexpr std::string_view k_make_list = "mk(0, []) :- !. mk(N, [N|T]) :- N1 is N - 1, mk(N1, T).";
constexpr std::string_view k_member = "member(X, [X|_]). member(X, [_|T]) :- member(X, T).";

// 例外を通さずに問い合わせる。ResourceLimit は失敗として数える（GC が無ければここへ来る）。
mosa::Stop ask(mosa::Machine& machine, std::string_view goal, bool& limited) {
  limited = false;
  try {
    return machine.query(goal);
  } catch (const mosa::ResourceLimit&) {
    limited = true;
    return mosa::Stop{};
  }
}

std::string binding_text(mosa::Machine& machine, const mosa::Stop& stop, std::size_t index) {
  if (index >= stop.bindings.size()) return {};
  return mosa::write_term(machine.heap(), machine.operators(), stop.bindings[index].variable, {}, {});
}

void check_loop(std::string_view label, std::string_view program) {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(program).ok, std::string{label} + ": load");
  auto& impl = mosa::engine_impl(*machine);
  const auto capacity = impl.gc_capacity;
  bool limited = false;
  const auto stop = ask(*machine, "loop(1000000).", limited);
  mosa_test::check(!limited, std::string{label} + ": loop does not hit heap_limit");
  mosa_test::check(stop.kind == mosa::StopKind::Success, std::string{label} + ": loop succeeds");
  mosa_test::check(impl.gc_stats.count >= 1, std::string{label} + ": GC ran");
  mosa_test::check(impl.gc_capacity == capacity, std::string{label} + ": soft capacity unchanged");
  mosa_test::check(impl.extras.size() < 1000, std::string{label} + ": extras plateau");
  mosa_test::check(impl.trail.slots.size() < 1000, std::string{label} + ": trail plateau");
}

void check_tak() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_tak).ok, "tak: load");
  bool limited = false;
  const auto stop = ask(*machine, "tak(18, 12, 6, A).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "tak(18,12,6,A) succeeds");
  mosa_test::check(binding_text(*machine, stop, 0) == "7", "tak(18,12,6,A) gives 7");
}

void check_cyclic() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "cyclic: load");
  bool limited = false;
  const auto stop = ask(*machine, "X = f(X), loop(200000), X = f(Y), Y == X.", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "cyclic term survives GC");
}

void check_long_live_list() {
  auto machine = mosa::Machine::create();
  std::string program{k_make_list};
  program += ' ';
  program += k_loop;
  mosa_test::check(machine->load_program(program).ok, "long list: load");
  bool limited = false;
  const auto stop = ask(*machine, "mk(2000000, L), loop(1000000), length(L, N), N == 2000000.", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "2,000,000 element list survives GC");
  mosa_test::check(mosa::engine_impl(*machine).gc_stats.count >= 1, "long list: GC ran");
}

void check_ball() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "ball: load");
  bool limited = false;
  const auto stop = ask(*machine,
                        "catch((T = g([1,2,3], \"abc\", 1.5e300, 4611686018427387904), loop(200000), throw(T)), "
                        "B, true), B == g([1,2,3], \"abc\", 1.5e300, 4611686018427387904).",
                        limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "ball keeps floats and boxed integers");
}

void check_redo() {
  auto machine = mosa::Machine::create(k_small);
  std::string program{k_member};
  program += ' ';
  program += k_loop;
  mosa_test::check(machine->load_program(program).ok, "redo: load");
  bool limited = false;
  auto stop = ask(*machine, "member(X, [a,b,c]), loop(200000).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success && binding_text(*machine, stop, 0) == "a",
                   "redo: first answer a");
  for (const std::string_view expected : {"b", "c"}) {
    try {
      stop = machine->next();
    } catch (const mosa::ResourceLimit&) {
      stop = mosa::Stop{};
    }
    mosa_test::check(stop.kind == mosa::StopKind::Success && binding_text(*machine, stop, 0) == expected,
                     "redo: next answer " + std::string{expected});
  }
  try {
    stop = machine->next();
  } catch (const mosa::ResourceLimit&) {
    stop = mosa::Stop{mosa::StopKind::Exception, 0, {}, {}, false};
  }
  mosa_test::check(stop.kind == mosa::StopKind::Failure, "redo: then no more");
}

void check_tiny_limit() {
  try {
    auto tiny = mosa::Machine::create(8);
    auto boom = tiny->query("catch(p(a), _, true).");
    mosa_test::check(boom.kind != mosa::StopKind::Success && boom.kind != mosa::StopKind::Exception,
                     "tiny limit stops without a prolog ball");
  } catch (const mosa::ResourceLimit&) {
    mosa_test::check(true, "tiny limit stops without a prolog ball");
  }
}

void check_direct_collect() {
  auto machine = mosa::Machine::create(k_small);
  auto& impl = mosa::engine_impl(*machine);
  const auto stop = machine->query("X = f(Y, [1|Z], 2.5).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "direct: query succeeds");
  const auto before_size = machine->heap().size();
  const auto before_text = binding_text(*machine, stop, 0);
  mosa::collect_garbage(impl);
  mosa_test::check(machine->heap().size() <= before_size, "direct: heap does not grow");
  // 呼ぶ前の Stop の束縛は古い番地なので、GC が書き換えた機械の束縛から読む。
  mosa::Stop after;
  after.bindings = impl.bindings;
  mosa_test::check(binding_text(*machine, after, 0) == before_text, "direct: binding reads the same");
  mosa_test::check(before_text.starts_with("f(") && before_text.find("2.5") != std::string::npos,
                   "direct: binding is the structure");
}

}  // namespace

int main() {
  check_loop("two goals", k_loop);
  check_loop("three goals", k_loop_three);
  check_tak();
  check_cyclic();
  check_long_live_list();
  check_ball();
  check_redo();
  check_tiny_limit();
  check_direct_collect();
  return mosa_test::failures();
}
