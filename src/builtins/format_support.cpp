// '$stream_column'(+stream_or_alias, -integer)。13211-1 に条項は無い。library(format) の ~N が使う。
//   Column は流れの最後の改行から書いた字の数で、0 から数える（stream_property/2 の
//   position(stream_position(_, _, LinePos)) の LinePos から 1 を引いた値）。字は Unicode のスカラー値 1 つを 1 と
//   数え、東アジア幅では数えない。
//   流れの検査は put_char/2 と同じ: instantiation_error、domain_error(stream_or_alias, S)、
//   existence_error(stream, S)、permission_error(output, stream, S)、permission_error(output, binary_stream, S)
// テスト: tests/pl/lib_format.pl、tests/cpp/format_det_test.cpp
#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>

namespace mosa {
namespace {

void prove_stream_column(Impl& machine, const Goal& goal) {
  std::size_t index = 0;
  if (select_output_stream(machine, goal, goal.arg(machine, 0), index)) return;
  // line_pos は行の頭で 1 になる（chars.cpp と write_pred.cpp の advance）。~N が知りたいのは行の頭かどうかなので、
  // 行の頭を 0 とする数え方で返す。
  const auto column = static_cast<std::int64_t>(machine.streams[index].line_pos - 1);
  if (do_unify(machine, goal.arg(machine, 1), machine.heap.integer(column))) machine.ip = goal.cont;
  else machine.failed = true;
}

}  // namespace

bool run_format_support(Impl& machine, const Goal& goal) {
  if (goal.arity != 2 || machine.atoms.name(goal.name) != "$stream_column") return false;
  prove_stream_column(machine, goal);
  return true;
}

}  // namespace mosa
