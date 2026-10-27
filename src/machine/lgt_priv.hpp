#pragma once

// lgt_judge と lgt_exec の境目。テストランナーの外には出さない。

#include "lgt_suite.hpp"

#include "mosa/machine.hpp"
#include "mosa/reader.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mosa {

struct LgtParsed {
  bool ok = false;
  Cell term{};
  std::vector<VarName> names;
};

struct LgtStep {
  bool dead = false;
  Stop stop;
};

[[nodiscard]] LgtParsed lgt_parse_term(Machine& machine, std::string_view text);
[[nodiscard]] bool lgt_check_text(Machine& machine, std::string_view text, const std::vector<Binding>& bindings,
                                  std::string& note);
[[nodiscard]] bool lgt_scratch_ok(Machine& machine, std::string_view goal, const std::vector<Binding>& bindings);
// 開いている query は潰さない。ゴールも利用者の節もセルのまま写す。
[[nodiscard]] bool lgt_run_copied_goal(Machine& main, Machine& scratch, Cell goal);
[[nodiscard]] LgtStep lgt_ask(Machine& machine, std::string_view goal);
[[nodiscard]] LgtStep lgt_again(Machine& machine);
[[nodiscard]] std::string lgt_actual_of(const Machine& machine, const Stop& stop);
[[nodiscard]] LgtEnd lgt_aborted();
[[nodiscard]] LgtEnd lgt_failed_step(const Machine& machine, const LgtStep& step);
[[nodiscard]] LgtEnd lgt_finish(Machine& machine, const LgtWant& want, const Stop& stop);
struct LgtIo;

[[nodiscard]] LgtEnd lgt_judge_mixed(Machine& machine, std::string_view body, const LgtWant& want, const LgtIo* io);

}  // namespace mosa
