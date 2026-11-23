// ADR 0012。決定的に抜けた catch/3 と *-> の Entry を選択点スタックから外す。上限 2^16 語の機械で、
// 1,000,000 段のループが heap_limit に当たらず、選択点スタックと catch_log が頭打ちになること。
// 上に選択点が残る catch は外さず、戻ればまた例外を捕まえること。
#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_small = std::size_t{1} << 16;

constexpr std::string_view k_program =
    "lw(0) :- !. lw(N) :- catch(true, _, true), N1 is N - 1, lw(N1).\n"
    "ls(0) :- !. ls(N) :- (true *-> true ; true), N1 is N - 1, ls(N1).\n"
    "ln(0) :- !. ln(N) :- catch((true *-> true ; true), _, true), N1 is N - 1, ln(N1).\n"
    "lc(0) :- !. lc(N) :- catch(catch(true, _, true), _, true), N1 is N - 1, lc(N1).\n"
    // ADR 0014。ゴール位置に変数があるので、Goal と Recovery は Call の節点として call/1 で動く。
    "lv(0, _) :- !. lv(N, X) :- catch((X, true), _, true), N1 is N - 1, lv(N1, X).\n"
    "lr(0, _) :- !. lr(N, X) :- catch(throw(e), _, (X, true)), N1 is N - 1, lr(N1, X).\n"
    "p(1). p(2).\n"
    // レビュー L-32。portray の中の catch は p/1 の選択点を上に残して抜けるので、無効にした印を catch_log に残す。
    "portray(l32) :- catch(p(_), _, true), write(px).\n"
    "lp(N) :- N > 0, print(l32), N1 is N - 1, lp(N1).\nlp(0).";

mosa::Stop ask(mosa::Machine& machine, std::string_view goal, bool& limited) {
  limited = false;
  try {
    return machine.query(goal);
  } catch (const mosa::ResourceLimit&) {
    limited = true;
    return mosa::Stop{};
  }
}

std::size_t count_pick(const mosa::Machine::Impl& impl, mosa::Pick pick) {
  std::size_t count = 0;
  for (const mosa::Entry& entry : impl.stack) {
    if (entry.pick == pick) ++count;
  }
  return count;
}

void check_loop(std::string_view goal) {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_program).ok, std::string{goal} + ": load");
  auto& impl = mosa::engine_impl(*machine);
  bool limited = false;
  const auto stop = ask(*machine, goal, limited);
  mosa_test::check(!limited, std::string{goal} + ": does not hit heap_limit");
  mosa_test::check(stop.kind == mosa::StopKind::Success, std::string{goal} + ": succeeds");
  mosa_test::check(impl.stack.size() < 16, std::string{goal} + ": choice stack stays flat");
  mosa_test::check(impl.catch_log.size() < 16, std::string{goal} + ": catch_log stays flat");
}

// 上に選択点が残る catch は外さない。次の解へ戻ると、また ball を捕まえる。
void check_kept_catch() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_program).ok, "kept: load");
  const auto& impl = mosa::engine_impl(*machine);
  auto stop = machine->query("catch((p(X), (X == 2 -> throw(b) ; true)), b, Y = caught).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "kept: first answer");
  mosa_test::check(count_pick(impl, mosa::Pick::Catch) == 1, "kept: catch entry remains above p/1");
  stop = machine->next();
  mosa_test::check(stop.kind == mosa::StopKind::Success, "kept: second answer caught by the re-armed catch");
}

// 決定的に抜けた catch と *-> は、その場で Entry を残さない。
void check_popped_now() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_program).ok, "popped: load");
  const auto& impl = mosa::engine_impl(*machine);
  auto stop = machine->query("catch(true, _, true), (true *-> true ; true), p(_).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "popped: first answer");
  mosa_test::check(count_pick(impl, mosa::Pick::Catch) == 0, "popped: no catch entry");
  mosa_test::check(count_pick(impl, mosa::Pick::ElseGoal) == 0, "popped: no else entry");
}

}  // namespace

// レビュー L-32。run_goal は入れ子の実行の後でスタックを戻すとき、catch_log も戻したスタックまで削る。前は入れ子の
// 枠の印が残り、後の切りや抜けまで portray を呼ぶたびに増えた。lp/1 は切りも後戻りもしないので、増えれば残る。
void check_portray_catch_log() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, "portray: load");
  auto& impl = mosa::engine_impl(*machine);
  bool limited = false;
  const auto stop = ask(*machine, "with_output_to(atom(_), lp(1000)).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "portray: succeeds");
  std::size_t above = 0;
  for (const auto& record : impl.catch_log) {
    if (record.index >= impl.stack.size()) ++above;
  }
  mosa_test::check(impl.catch_log.size() < 16, "portray: catch_log stays flat, got " +
                                                    std::to_string(impl.catch_log.size()));
  mosa_test::check(above == 0, "portray: no record above the stack");
}

int main() {
  check_loop("lw(1000000).");
  check_loop("ls(1000000).");
  check_loop("ln(1000000).");
  check_loop("lc(1000000).");
  check_loop("lv(1000000, true).");
  check_loop("lr(1000000, true).");
  check_kept_catch();
  check_popped_now();
  check_portray_catch_log();
  return mosa_test::failures();
}
