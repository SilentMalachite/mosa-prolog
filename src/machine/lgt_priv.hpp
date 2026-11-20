#pragma once

// lgt_judge と lgt_exec の境目。テストランナーの外には出さない。

#include "lgt_suite.hpp"

#include "mosa/machine.hpp"
#include "mosa/reader.hpp"

#include <optional>
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

// 整数、浮動小数点数、前置の - と + の付いた数。lgt_approx.cpp。
[[nodiscard]] std::optional<double> lgt_as_number(const Heap& heap, Cell cell);
[[nodiscard]] bool approx(const Heap& heap, Cell left, Cell right);
[[nodiscard]] LgtParsed lgt_parse_term(Machine& machine, std::string_view text);
[[nodiscard]] bool lgt_check_text(Machine& machine, std::string_view text, const std::vector<Binding>& bindings,
                                  std::string& note);
// スクラッチの機械で動かした結果。Stopped は例外か halt で止まったことで、どちらかは stop が持つ。例外の項は
// 元の機械のヒープに読み直してある。例外と halt を失敗に潰すと false と all の試験が通る（レビュー H-36）。
// Dead は結果を持ち帰れなかった（ヒープの上限、読み直せない項）。主の機械の上限と同じく打ち切りにする。
enum class LgtScratchKind { Ok, No, Stopped, Dead };

struct LgtScratch {
  LgtScratchKind kind = LgtScratchKind::No;
  Stop stop;
};

[[nodiscard]] LgtScratch lgt_scratch_run(Machine& machine, std::string_view goal, const std::vector<Binding>& bindings);
// 開いている query は潰さない。ゴールも利用者の節もセルのまま写す。
[[nodiscard]] LgtScratch lgt_run_copied_goal(Machine& main, Machine& scratch, Cell goal);
[[nodiscard]] LgtStep lgt_ask(Machine& machine, std::string_view goal);
[[nodiscard]] LgtStep lgt_again(Machine& machine);
[[nodiscard]] std::string lgt_actual_of(const Machine& machine, const Stop& stop);
[[nodiscard]] LgtEnd lgt_aborted();
[[nodiscard]] LgtEnd lgt_failed_step(const Machine& machine, const LgtStep& step);
[[nodiscard]] LgtEnd lgt_finish(Machine& machine, const LgtWant& want, const Stop& stop);
struct LgtIo;

[[nodiscard]] LgtEnd lgt_judge_mixed(Machine& machine, std::string_view body, const LgtWant& want, const LgtIo* io);

}  // namespace mosa
