// ヒープが予約の上で伸びること、利用者の決めた上限で止まること、上限の決め方（ADR 0010）。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/machine.hpp"

#include <optional>
#include <string_view>

namespace {

constexpr std::string_view k_make_list = "mk(0, []) :- !. mk(N, [N|T]) :- N1 is N - 1, mk(N1, T).";

void check_grows_past_initial() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_make_list).ok, "load mk");
  const auto* base = machine->heap().data();
  mosa_test::check(machine->heap().committed() <= mosa::kInitialHeapCommit, "starts at the initial commit");
  const auto stop = machine->query("mk(3000000, L).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "3,000,000 element list fits");
  mosa_test::check(machine->heap().committed() > mosa::kInitialHeapCommit, "heap committed more");
  mosa_test::check(machine->heap().data() == base, "growing does not move the heap");
}

void check_limit_stops() {
  auto machine = mosa::Machine::create(std::size_t{1} << 16);
  mosa_test::check(machine->load_program(k_make_list).ok, "load mk (small)");
  bool limited = false;
  try {
    (void)machine->query("mk(3000000, L).");
  } catch (const mosa::ResourceLimit&) {
    limited = true;
  }
  mosa_test::check(limited, "the user's ceiling stops the query");
  machine->heap().truncate(0);
  mosa_test::check(machine->heap().committed() <= (std::size_t{1} << 16), "never commits past the ceiling");
  mosa_test::check(machine->query("X = 1.").kind == mosa::StopKind::Success, "usable after the stop");
}

void check_parse() {
  mosa_test::check(mosa::parse_heap_max_mib("64") == (std::size_t{64} << 17), "64 MiB in words");
  mosa_test::check(mosa::parse_heap_max_mib("1") == (std::size_t{1} << 17), "1 MiB in words");
  for (const std::string_view bad : {"0", "-1", "+1", "", "1x", " 1", "1.5", "99999999999999999999"}) {
    mosa_test::check(!mosa::parse_heap_max_mib(bad).has_value(), "invalid heap max is rejected");
  }
  mosa_test::check(mosa::heap_limit_mib_text(std::size_t{16} << 17) == "16", "exact MiB text");
  mosa_test::check(mosa::heap_limit_mib_text(std::size_t{1} << 16) == "1", "tiny limit rounds up to 1");
  mosa_test::check(mosa::heap_limit_mib_text(mosa::kDefaultHeapMax) == "4096", "default is 4096 MiB");
}

void check_resolve_order() {
  using View = std::optional<std::string_view>;
  mosa_test::check(mosa::resolve_heap_max(View{"8"}, View{"16"}) == (std::size_t{8} << 17), "option beats env");
  mosa_test::check(mosa::resolve_heap_max(std::nullopt, View{"16"}) == (std::size_t{16} << 17), "env alone");
  mosa_test::check(mosa::resolve_heap_max(std::nullopt, std::nullopt) == mosa::kDefaultHeapMax, "default");
  mosa_test::check(mosa::resolve_heap_max(std::nullopt, View{"junk"}) == mosa::kDefaultHeapMax,
                   "unreadable env falls back to the default");
}

}  // namespace

int main() {
  check_grows_past_initial();
  check_limit_stops();
  check_parse();
  check_resolve_order();
  return mosa_test::failures();
}
